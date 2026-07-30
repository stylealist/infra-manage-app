/***************************************************************************
  projectutils.cpp - ProjectUtils

 ---------------------
 begin                : 19.04.2024
 copyright            : (C) 2024 by Mathieu Pellerin
 email                : mathieu@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "layerutils.h"
#include "platformutilities.h"
#include "positioningutils.h"
#include "projectutils.h"

#include <qgsattributeeditorcontainer.h>
#include <qgsattributeeditorfield.h>
#include <qgsattributeeditorrelation.h>
#include <qgsmaplayer.h>
#include <qgsprojectdisplaysettings.h>
#include <qgsrasterlayer.h>
#include <qgsrelationcontext.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectortilelayer.h>
#include <qgsvectortileutils.h>

// 생성자: 부모 QObject를 초기화
ProjectUtils::ProjectUtils( QObject *parent )
  : QObject( parent )
{
}

// 프로젝트에 포함된 모든 맵 레이어를 QVariantMap 형태로 반환
// key: 레이어 ID, value: QgsMapLayer 포인터
QVariantMap ProjectUtils::mapLayers( QgsProject *project )
{
  if ( !project )
    return QVariantMap();

  QVariantMap mapLayers;
  const QMap<QString, QgsMapLayer *> projectMapLayers = project->mapLayers();
  for ( const QString &layerId : projectMapLayers.keys() )
  {
    mapLayers.insert( layerId, QVariant::fromValue<QgsMapLayer *>( projectMapLayers[layerId] ) );
  }

  return mapLayers;
}

// 프로젝트에 맵 레이어를 추가하고 성공 여부를 반환
bool ProjectUtils::addMapLayer( QgsProject *project, QgsMapLayer *layer )
{
  if ( !project )
    return false;

  return ( project->addMapLayer( layer ) );
}

// 레이어 포인터로 맵 레이어를 프로젝트에서 제거
void ProjectUtils::removeMapLayer( QgsProject *project, QgsMapLayer *layer )
{
  if ( !project || !layer )
    return;

  project->removeMapLayer( layer );
}

// 레이어 ID 문자열로 맵 레이어를 프로젝트에서 제거
void ProjectUtils::removeMapLayer( QgsProject *project, const QString &layerId )
{
  if ( !project || layerId.isEmpty() )
    return;

  project->removeMapLayer( layerId );
}

// 프로젝트의 트랜잭션 모드를 반환
// 프로젝트가 없으면 Disabled 반환
Qgis::TransactionMode ProjectUtils::transactionMode( QgsProject *project )
{
  if ( !project )
    return Qgis::TransactionMode::Disabled;

  return project->transactionMode();
}

// 프로젝트 제목을 반환
// 제목이 없으면 파일명(확장자 제외)을 반환
QString ProjectUtils::title( QgsProject *project )
{
  if ( !project )
    return QString();

  const QString title = project->title();
  return !title.isEmpty() ? title : QFileInfo( project->fileName() ).completeBaseName();
}

// 새 QField 프로젝트를 생성하고 저장된 파일 경로를 반환
// options: 프로젝트 구성 옵션 맵 (메모, 트랙, 기본지도 등)
// positionInformation: 현재 GPS 위치 정보 (초기 지도 범위 설정에 활용)
QString ProjectUtils::createProject( const QVariantMap &options, const GnssPositionInformation &positionInformation )
{
  // 프로젝트 제목을 가져오고, 파일명으로 사용할 수 있도록 특수문자를 언더스코어로 치환
  QString projectTitle = options.value( QStringLiteral( "title" ), tr( "Created Project" ) ).toString();
  QString projectFilename = projectTitle;
  projectFilename.replace( QRegularExpression( "[^A-Za-z0-9_]" ), QStringLiteral( "_" ) );

  // 프로젝트 디렉토리 경로 설정; 동일한 이름이 존재하면 _2, _3 등의 접미사를 붙여 고유 경로 생성
  QDir createdProjectsDir( QStringLiteral( "%1/Created Projects/" ).arg( PlatformUtilities::instance()->applicationDirectory() ) );
  QString createdProjectDir = createdProjectsDir.filePath( projectFilename );
  int uniqueSuffix = 2;
  while ( QFileInfo::exists( createdProjectDir ) )
  {
    createdProjectDir = QStringLiteral( "%1_%2" ).arg( createdProjectsDir.filePath( projectFilename ), QString::number( uniqueSuffix++ ) );
  }
  createdProjectDir = QDir::cleanPath( createdProjectDir );
  createdProjectsDir.mkpath( createdProjectDir );
  const QString projectFilepath = QStringLiteral( "%1/%2.qgz" ).arg( createdProjectDir, projectFilename );

  // 프로젝트에 추가할 레이어 목록 및 새 프로젝트 객체 초기화
  QList<QgsMapLayer *> createdProjectLayers;
  QgsProject *createdProject = new QgsProject();

  // 기본 좌표계 설정: 저장은 EPSG:3857(Web Mercator), 화면 표시는 EPSG:4326(WGS84)
  createdProject->setCrs( QgsCoordinateReferenceSystem( "EPSG:3857" ) );
  createdProject->displaySettings()->setCoordinateType( Qgis::CoordinateDisplayType::CustomCrs );
  createdProject->displaySettings()->setCoordinateCustomCrs( QgsCoordinateReferenceSystem( "EPSG:4326" ) );

  // ── 메모 레이어 생성 ──────────────────────────────────────────────────
  QgsVectorLayer *notesLayer = nullptr;
  QgsVectorLayer *attachmentsLayer = nullptr;
  if ( options.value( QStringLiteral( "notes" ) ).toBool() )
  {
    // 초기 지도 모드를 디지타이징 모드로 설정
    createdProject->writeEntry( QStringLiteral( "qfieldsync" ), QStringLiteral( "initialMapMode" ), QStringLiteral( "digitize" ) );

    const QString notesFilepath = QStringLiteral( "%1/notes.gpkg" ).arg( createdProjectDir );

    // 메모 레이어 필드 정의: uuid(고유키), color(색상), title(제목), note(내용), timestamp(시간)
    QgsFields fields;
    fields.append( QgsField( QStringLiteral( "uuid" ), QMetaType::QString ) );
    fields.append( QgsField( QStringLiteral( "color" ), QMetaType::QString ) );
    //fields.append( QgsField( QStringLiteral( "title" ), QMetaType::QString ) );
    fields.append( QgsField( QStringLiteral( "facility_nm" ), QMetaType::QString ) );
    fields.append( QgsField( QStringLiteral( "note" ), QMetaType::QString ) );
    fields.append( QgsField( QStringLiteral( "timestamp" ), QMetaType::QDateTime ) );

    // GeoPackage 파일로 메모 레이어 생성 (포인트Z 타입, WGS84)
    QgsVectorFileWriter::SaveVectorOptions writerOptions;
    QgsVectorFileWriter *writer = QgsVectorFileWriter::create( notesFilepath, fields, Qgis::WkbType::PointZ, QgsCoordinateReferenceSystem( "EPSG:4326" ), createdProject->transformContext(), writerOptions );
    delete writer;

    // 메모 레이어 로드 및 기본 렌더러/레이블 설정
    // 카메라 캡처 활성화 시 첨부파일 관계 집계 표현식 사용
    notesLayer = new QgsVectorLayer( notesFilepath, tr( "Notes" ) );
    fields = notesLayer->fields();
    LayerUtils::setDefaultRenderer( notesLayer, nullptr,
                                    options.value( QStringLiteral( "camera_capture" ) ).toBool() ? QStringLiteral( "relation_aggregate('notes_attachments_relation', 'max', \"media\")" ) : QString(),
                                    QStringLiteral( "color" ) );
    LayerUtils::setDefaultLabeling( notesLayer );

    // 피처 목록에 표시될 표현식 설정: 제목이 없으면 "Note #번호 from 날짜" 형식
    //notesLayer->setDisplayExpression( "COALESCE( title , 'Note #' || fid || ' from ' || format_date( timestamp, 'yyyy-MM-dd HH:mm' ) )" );
    notesLayer->setDisplayExpression( "COALESCE( facility_nm , 'Note #' || fid || ' from ' || format_date( timestamp, 'yyyy-MM-dd HH:mm' ) )" );

    int fieldIndex;
    QVariantMap widgetOptions;
    QgsEditorWidgetSetup widgetSetup;

    // fid 필드: 사용자에게 숨김 처리
    fieldIndex = fields.indexOf( QStringLiteral( "fid" ) );
    if ( fieldIndex >= 0 )
    {
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Hidden" ), widgetOptions );
      notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
    }

    // uuid 필드: 숨김 처리 + 자동으로 uuid() 함수로 기본값 생성
    fieldIndex = fields.indexOf( QStringLiteral( "uuid" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Hidden" ), widgetOptions );
      notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      notesLayer->setDefaultValueDefinition( fieldIndex, QgsDefaultValue( QStringLiteral( "uuid()" ), false ) );
    }

    // timestamp 필드: 날짜/시간 위젯 설정, 기본값은 현재 시각(now())
    fieldIndex = fields.indexOf( QStringLiteral( "timestamp" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetOptions[QStringLiteral( "display_format" )] = QStringLiteral( "yyyy-MM-dd HH:mm" );
      widgetOptions[QStringLiteral( "field_format" )] = QStringLiteral( "yyyy-MM-dd HH:mm" );
      widgetOptions[QStringLiteral( "field_format_overwrite" )] = true;
      widgetOptions[QStringLiteral( "allow_null" )] = true;
      widgetOptions[QStringLiteral( "calendar_popup" )] = true;
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "DateTime" ), widgetOptions );
      notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      notesLayer->setDefaultValueDefinition( fieldIndex, QgsDefaultValue( QStringLiteral( "now()" ), false ) );
      notesLayer->setFieldAlias( fieldIndex, tr( "Time" ) );
    }

    // color 필드: 색상 선택 위젯, 기본값은 파란색(#377eb8)
    fieldIndex = fields.indexOf( QStringLiteral( "color" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Color" ), widgetOptions );
      notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      notesLayer->setDefaultValueDefinition( fieldIndex, QgsDefaultValue( QStringLiteral( "'#377eb8'" ), false ) );
      notesLayer->setFieldAlias( fieldIndex, tr( "Marker color" ) );
    }

    // title 필드: 일반 텍스트 입력 위젯
    // fieldIndex = fields.indexOf( QStringLiteral( "title" ) );
    // if ( fieldIndex >= 0 )
    // {
    //   widgetOptions.clear();
    //   widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), widgetOptions );
    //   notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
    //   notesLayer->setFieldAlias( fieldIndex, tr( "Title" ) );
    // }
    fieldIndex = fields.indexOf( QStringLiteral( "facility_nm" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), widgetOptions );
      tracksLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      tracksLayer->setFieldAlias( fieldIndex, tr( "시설물명" ) );
    }

    // note 필드: 여러 줄 입력이 가능한 텍스트 위젯
    fieldIndex = fields.indexOf( QStringLiteral( "note" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetOptions[QStringLiteral( "IsMultiline" )] = true;
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), widgetOptions );
      notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      notesLayer->setFieldAlias( fieldIndex, tr( "Note" ) );
    }

    // QFieldCloud 오프라인 동기화를 위한 커스텀 속성 설정
    notesLayer->setCustomProperty( QStringLiteral( "QFieldSync/cloud_action" ), QStringLiteral( "offline" ) );
    notesLayer->setCustomProperty( QStringLiteral( "QFieldSync/action" ), QStringLiteral( "offline" ) );

    // 피처 목록 표시 표현식을 title 필드로 설정
    //notesLayer->setDisplayExpression( QStringLiteral( "\"title\"" ) );
    notesLayer->setDisplayExpression( QStringLiteral( "\"facility_nm\"" ) );

    createdProjectLayers << notesLayer;

    // ── 첨부파일 자식 레이어 생성 (카메라 캡처 옵션 활성화 시) ─────────
    if ( options.value( QStringLiteral( "camera_capture" ) ).toBool() )
    {
      // 동일한 notes.gpkg 파일에 두 번째 레이어(notes_attachments)로 추가
      QgsFields attachFields;
      attachFields.append( QgsField( QStringLiteral( "note_uuid" ), QMetaType::QString ) );  // 부모 메모와 연결하는 외래키
      attachFields.append( QgsField( QStringLiteral( "media" ), QMetaType::QString ) );       // 미디어 파일 경로
      attachFields.append( QgsField( QStringLiteral( "description" ), QMetaType::QString ) ); // 첨부파일 설명
      attachFields.append( QgsField( QStringLiteral( "timestamp" ), QMetaType::QDateTime ) ); // 첨부 시각

      // 지오메트리 없는 테이블로 첨부파일 레이어 생성
      QgsVectorFileWriter::SaveVectorOptions attachWriterOptions;
      attachWriterOptions.layerName = QStringLiteral( "notes_attachments" );
      attachWriterOptions.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
      QgsVectorFileWriter *attachWriter = QgsVectorFileWriter::create(
        notesFilepath, attachFields, Qgis::WkbType::NoGeometry,
        QgsCoordinateReferenceSystem(),
        createdProject->transformContext(), attachWriterOptions );
      delete attachWriter;

      // 생성된 첨부파일 레이어 로드
      const QString attachUri = QStringLiteral( "%1|layername=notes_attachments" ).arg( notesFilepath );
      attachmentsLayer = new QgsVectorLayer( attachUri, tr( "Note attachments" ) );
      QgsFields liveAttachFields = attachmentsLayer->fields();

      int attachFieldIndex;
      QVariantMap attachWidgetOptions;
      QgsEditorWidgetSetup attachWidgetSetup;

      // fid 필드: 숨김 처리
      attachFieldIndex = liveAttachFields.indexOf( QStringLiteral( "fid" ) );
      if ( attachFieldIndex >= 0 )
      {
        attachWidgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Hidden" ), QVariantMap());
        attachmentsLayer->setEditorWidgetSetup( attachFieldIndex, attachWidgetSetup );
      }

      // note_uuid 필드: 부모 메모 참조키로 숨김 처리
      attachFieldIndex = liveAttachFields.indexOf( QStringLiteral( "note_uuid" ) );
      if ( attachFieldIndex >= 0 )
      {
        attachWidgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Hidden" ), QVariantMap() );
        attachmentsLayer->setEditorWidgetSetup( attachFieldIndex, attachWidgetSetup );
      }

      // media 필드: 파일 선택 위젯으로 설정 (문서 뷰어, 상대 경로 저장)
      attachFieldIndex = liveAttachFields.indexOf( QStringLiteral( "media" ) );
      if ( attachFieldIndex >= 0 )
      {
        attachWidgetOptions.clear();
        attachWidgetOptions[QStringLiteral( "DocumentViewer" )] = 1;
        attachWidgetOptions[QStringLiteral( "RelativeStorage" )] = 1;
        attachWidgetOptions[QStringLiteral( "FileWidget" )] = true;
        attachWidgetOptions[QStringLiteral( "FileWidgetButton" )] = true;
        attachWidgetSetup = QgsEditorWidgetSetup( QStringLiteral( "ExternalResource" ), attachWidgetOptions );
        attachmentsLayer->setEditorWidgetSetup( attachFieldIndex, attachWidgetSetup );
        attachmentsLayer->setFieldAlias( attachFieldIndex, tr( "Media" ) );
      }

      // description 필드: 여러 줄 텍스트 입력 위젯
      attachFieldIndex = liveAttachFields.indexOf( QStringLiteral( "description" ) );
      if ( attachFieldIndex >= 0 )
      {
        attachWidgetOptions.clear();
        attachWidgetOptions[QStringLiteral( "IsMultiline" )] = true;
        attachWidgetSetup = QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), attachWidgetOptions );
        attachmentsLayer->setEditorWidgetSetup( attachFieldIndex, attachWidgetSetup );
        attachmentsLayer->setFieldAlias( attachFieldIndex, tr( "Description" ) );
      }

      // timestamp 필드: 날짜/시간 위젯, 기본값은 현재 시각(now())
      attachFieldIndex = liveAttachFields.indexOf( QStringLiteral( "timestamp" ) );
      if ( attachFieldIndex >= 0 )
      {
        attachWidgetOptions.clear();
        attachWidgetOptions[QStringLiteral( "display_format" )] = QStringLiteral( "yyyy-MM-dd HH:mm" );
        attachWidgetOptions[QStringLiteral( "field_format" )] = QStringLiteral( "yyyy-MM-dd HH:mm" );
        attachWidgetOptions[QStringLiteral( "field_format_overwrite" )] = true;
        attachWidgetOptions[QStringLiteral( "allow_null" )] = true;
        attachWidgetOptions[QStringLiteral( "calendar_popup" )] = true;
        attachWidgetSetup = QgsEditorWidgetSetup( QStringLiteral( "DateTime" ), attachWidgetOptions );
        attachmentsLayer->setEditorWidgetSetup( attachFieldIndex, attachWidgetSetup );
        attachmentsLayer->setDefaultValueDefinition( attachFieldIndex, QgsDefaultValue( QStringLiteral( "now()" ), false ) );
        attachmentsLayer->setFieldAlias( attachFieldIndex, tr( "Time" ) );
      }

      // 첨부파일 표시 표현식: media가 없으면 "Attachment #번호" 형식
      attachmentsLayer->setDisplayExpression( QStringLiteral( "COALESCE(\"media\", 'Attachment #' || fid)" ) );

      // 속성 폼 자동 억제(피처 추가 시 폼 생략)
      QgsEditFormConfig attachFormConfig = attachmentsLayer->editFormConfig();
      attachFormConfig.setSuppress( Qgis::AttributeFormSuppression::On );
      attachmentsLayer->setEditFormConfig( attachFormConfig );

      // QFieldCloud 오프라인 동기화 설정
      attachmentsLayer->setCustomProperty( QStringLiteral( "QFieldSync/cloud_action" ), QStringLiteral( "offline" ) );
      attachmentsLayer->setCustomProperty( QStringLiteral( "QFieldSync/action" ), QStringLiteral( "offline" ) );

      // 첨부파일 레이어를 비공개(Private)로 설정하여 범례에서 숨김
      attachmentsLayer->setFlags( attachmentsLayer->flags() | QgsMapLayer::Private );

      createdProjectLayers << attachmentsLayer;

      // 메모 레이어 폼 레이아웃을 드래그앤드롭 방식으로 재구성
      // 첨부파일 관계 위젯과 주요 필드를 순서대로 배치
      QgsEditFormConfig notesFormConfig = notesLayer->editFormConfig();
      notesFormConfig.clearTabs();
      notesFormConfig.setLayout( Qgis::AttributeFormLayout::DragAndDrop );
      QgsAttributeEditorContainer *root = notesFormConfig.invisibleRootContainer();
      
      // const QStringList orderedFields = {
      //   QStringLiteral( "title" ),
      //   QStringLiteral( "note" ),
      //   QStringLiteral( "color" ),
      //   QStringLiteral( "timestamp" ), };
      const QStringList orderedFields = {
      QStringLiteral( "facility_nm" ),
      QStringLiteral( "note" ),
      QStringLiteral( "color" ),
      QStringLiteral( "timestamp" ), };
      for ( const QString &fieldName : orderedFields )
      {
        const int idx = notesLayer->fields().indexOf( fieldName );
        if ( idx >= 0 )
        {
          root->addChildElement( new QgsAttributeEditorField( fieldName, idx, root ) );
        }
      }

      QgsAttributeEditorRelation *relationElement = new QgsAttributeEditorRelation( QStringLiteral( "notes_attachments_relation" ), root );
      root->addChildElement( relationElement );
      notesLayer->setEditFormConfig( notesFormConfig );
    }
  }

  // ── 트랙 레이어 생성 ─────────────────────────────────────────────────
  QgsVectorLayer *tracksLayer = nullptr;
  if ( options.value( QStringLiteral( "tracks" ) ).toBool() )
  {
    const QString tracksFilepath = QStringLiteral( "%1/tracks.gpkg" ).arg( createdProjectDir );

    // 트랙 레이어 필드 정의: color(트랙 색상), title(제목), timestamp(기록 시각)
    QgsFields fields;
    fields.append( QgsField( QStringLiteral( "color" ), QMetaType::QString ) );
    fields.append( QgsField( QStringLiteral( "facility_nm" ), QMetaType::QString ) );
    //fields.append( QgsField( QStringLiteral( "title" ), QMetaType::QString ) );
    fields.append( QgsField( QStringLiteral( "timestamp" ), QMetaType::QDateTime ) );

    // LineStringZM 타입으로 트랙 레이어 생성 (Z=고도, M=시간값)
    QgsVectorFileWriter::SaveVectorOptions writerOptions;
    QgsVectorFileWriter *writer = QgsVectorFileWriter::create( tracksFilepath, fields, Qgis::WkbType::LineStringZM, QgsCoordinateReferenceSystem( "EPSG:4326" ), createdProject->transformContext(), writerOptions );
    delete writer;

    // 트랙 레이어 로드 및 기본 렌더러 설정
    tracksLayer = new QgsVectorLayer( tracksFilepath, tr( "Tracks" ) );
    fields = tracksLayer->fields();
    LayerUtils::setDefaultRenderer( tracksLayer, nullptr, QString(), QStringLiteral( "color" ) );

    // 피처 목록 표시 표현식: "Track #번호 from 날짜" 형식
    tracksLayer->setDisplayExpression( "'Track #' || fid || ' from ' || format_date( timestamp, 'yyyy-MM-dd HH:mm' )" );

    int fieldIndex;
    QVariantMap widgetOptions;
    QgsEditorWidgetSetup widgetSetup;

    // fid 필드: 숨김 처리
    fieldIndex = fields.indexOf( QStringLiteral( "fid" ) );
    if ( fieldIndex >= 0 )
    {
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Hidden" ), widgetOptions );
      tracksLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
    }

    // color 필드: 색상 선택 위젯, 기본값은 파란색(#377eb8)
    fieldIndex = fields.indexOf( QStringLiteral( "color" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "Color" ), widgetOptions );
      tracksLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      tracksLayer->setDefaultValueDefinition( fieldIndex, QgsDefaultValue( QStringLiteral( "'#377eb8'" ), false ) );
      tracksLayer->setFieldAlias( fieldIndex, tr( "Track color" ) );
    }

    // title 필드: 일반 텍스트 입력 위젯
    // fieldIndex = fields.indexOf( QStringLiteral( "title" ) );
    // if ( fieldIndex >= 0 )
    // {
    //   widgetOptions.clear();
    //   widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), widgetOptions );
    //   tracksLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
    //   tracksLayer->setFieldAlias( fieldIndex, tr( "Title" ) );
    // }
    fieldIndex = fields.indexOf( QStringLiteral( "facility_nm" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "TextEdit" ), widgetOptions );
      notesLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      notesLayer->setFieldAlias( fieldIndex, tr( "시설물명" ) );
    }

    // timestamp 필드: 날짜/시간 위젯, 기본값은 현재 시각(now())
    fieldIndex = fields.indexOf( QStringLiteral( "timestamp" ) );
    if ( fieldIndex >= 0 )
    {
      widgetOptions.clear();
      widgetOptions[QStringLiteral( "display_format" )] = QStringLiteral( "yyyy-MM-dd HH:mm" );
      widgetOptions[QStringLiteral( "field_format" )] = QStringLiteral( "yyyy-MM-dd HH:mm" );
      widgetOptions[QStringLiteral( "field_format_overwrite" )] = true;
      widgetOptions[QStringLiteral( "allow_null" )] = true;
      widgetOptions[QStringLiteral( "calendar_popup" )] = true;
      widgetSetup = QgsEditorWidgetSetup( QStringLiteral( "DateTime" ), widgetOptions );
      tracksLayer->setEditorWidgetSetup( fieldIndex, widgetSetup );
      tracksLayer->setDefaultValueDefinition( fieldIndex, QgsDefaultValue( QStringLiteral( "now()" ), false ) );
      tracksLayer->setFieldAlias( fieldIndex, tr( "Time" ) );
    }

    if ( options.value( QStringLiteral( "track_on_launch" ) ).toBool() )
    {
      // 앱 실행 즉시 자동 추적 시작: 속성 폼 억제
      QgsEditFormConfig formConfig = tracksLayer->editFormConfig();
      formConfig.setSuppress( Qgis::AttributeFormSuppression::On );
      tracksLayer->setEditFormConfig( formConfig );

      // 추적 세션 자동 시작 설정
      // - 2초 간격으로 꼭짓점 기록
      // - 50m 초과 이동 시 오류 거리 보호(erroneous distance safeguard) 적용
      // - M값에 에포크(epoch) 타임스탬프 첨부
      tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/tracking_session_active" ), true );
      tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/tracking_time_requirement_active" ), true );
      tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/tracking_time_requirement_interval_seconds" ), 2 );
      tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/tracking_erroneous_distance_safeguard_active" ), true );
      tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/tracking_erroneous_distance_safeguard_maximum_meters" ), 50 );
      tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/tracking_measurement_type" ), 1 ); // M값에 에포크 타임스탬프 첨부
    }
    else
    {
      // 자동 추적이 아닌 경우 초기 지도 모드를 디지타이징으로 설정
      createdProject->writeEntry( QStringLiteral( "qfieldsync" ), QStringLiteral( "initialMapMode" ), QStringLiteral( "digitize" ) );
    }

    // QFieldCloud 오프라인 동기화 설정
    tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/cloud_action" ), QStringLiteral( "offline" ) );
    tracksLayer->setCustomProperty( QStringLiteral( "QFieldSync/action" ), QStringLiteral( "offline" ) );

    createdProjectLayers << tracksLayer;
  }

  // ── 기본 지도(Basemap) 레이어 생성 ──────────────────────────────────
  QgsMapLayer *basemapLayer = nullptr;
  QgsRectangle basemapExtent;
  const QString basemap = options.value( QStringLiteral( "basemap" ), QStringLiteral( "color" ) ).toString();
  const QString basemapCustomProvider = options.value( QStringLiteral( "basemap_custom_provider" ) ).toString();
  const QString basemapCustomExtent = options.value( QStringLiteral( "basemap_custom_extent" ) ).toString();
  QString basemapCustomSource = options.value( QStringLiteral( "basemap_custom_source" ) ).toString();

  // 기본 제공 배경 지도: colorful / darkgray / lightgray
  if ( basemap.compare( QStringLiteral( "colorful" ) ) == 0 || basemap.compare( QStringLiteral( "darkgray" ) ) == 0 || basemap.compare( QStringLiteral( "lightgray" ) ) == 0 )
  {
    basemapLayer = LayerUtils::createBasemap( basemap );
    // 테마에 맞는 배경색 설정
    if ( basemap.compare( QStringLiteral( "darkgray" ) ) == 0 )
    {
      createdProject->setBackgroundColor( QColor( 15, 15, 15 ) );
    }
    else if ( basemap.compare( QStringLiteral( "lightgray" ) ) == 0 )
    {
      createdProject->setBackgroundColor( QColor( 240, 240, 240 ) );
    }
    else
    {
      createdProject->setBackgroundColor( QColor( 242, 239, 233 ) );
    }
  }
  // 사용자 지정 URL 기본 지도: 벡터 타일 또는 래스터 레이어로 생성
  else if ( basemap.compare( QStringLiteral( "custom" ) ) == 0 || ( !basemapCustomSource.isEmpty() && !basemapCustomProvider.isEmpty() ) )
  {
    if ( basemapCustomProvider.toLower() == QStringLiteral( "vectortile" ) )
    {
      // 벡터 타일 기본 지도: URI 소스 업데이트 후 기본 스타일 로드
      QgsVectorTileUtils::updateUriSources( basemapCustomSource );
      QgsVectorTileLayer *layer = new QgsVectorTileLayer( basemapCustomSource, tr( "Basemap" ) );
      QString error;
      QStringList warnings;
      QList<QgsMapLayer *> subLayers;
      layer->loadDefaultStyleAndSubLayers( error, warnings, subLayers );
      basemapLayer = layer;
    }
    else
    {
      // 래스터 기본 지도 (XYZ 타일, WMS 등)
      basemapLayer = new QgsRasterLayer( basemapCustomSource, tr( "Basemap" ), basemapCustomProvider );
    }

    // 기본 지도의 범위 설정 (사용자 지정 범위가 있으면 우선 적용)
    basemapExtent = basemapLayer->extent();
    if ( !basemapCustomExtent.isEmpty() )
    {
      const QgsRectangle customExtent = QgsRectangle::fromWkt( basemapCustomExtent );
      if ( !customExtent.isEmpty() )
      {
        basemapExtent = customExtent;
      }
    }
  }

  // 유효한 기본 지도가 있으면 프로젝트의 좌표계와 초기 범위를 기본 지도에 맞게 설정
  QgsRectangle createdProjectExtent;
  if ( basemapLayer && basemapLayer->isValid() )
  {
    createdProjectLayers << basemapLayer;
    createdProject->setCrs( basemapLayer->crs() );
    createdProjectExtent = basemapExtent;
  }

  // QFieldCloud 클라우드 프로젝트를 위한 첨부파일 디렉토리 사전 등록
  createdProject->writeEntry( QStringLiteral( "qfieldsync" ), QStringLiteral( "attachmentDirs" ), QStringList() << "DCIM"
                                                                                                                << "audio"
                                                                                                                << "video"
                                                                                                                << "files" );

  // 모든 레이어를 프로젝트에 추가
  createdProject->addMapLayers( createdProjectLayers );

  // ── 메모-첨부파일 관계(Relation) 등록 ───────────────────────────────
  // notesLayer의 uuid 필드와 attachmentsLayer의 note_uuid 필드를 연결
  if ( notesLayer && attachmentsLayer )
  {
    QgsRelationContext relationContext( createdProject );
    QgsRelation rel( relationContext );
    rel.setId( QStringLiteral( "notes_attachments_relation" ) );
    rel.setName( tr( "Attachments" ) );
    rel.setReferencedLayer( notesLayer->id() );
    rel.setReferencingLayer( attachmentsLayer->id() );
    rel.addFieldPair( QStringLiteral( "note_uuid" ), QStringLiteral( "uuid" ) );
    rel.setStrength( Qgis::RelationshipStrength::Association );
    if ( rel.isValid() )
    {
      createdProject->relationManager()->addRelation( rel );
    }
  }

  // ── 프로젝트 저장 시 지도 캔버스 범위 XML 기록 ──────────────────────
  // GPS 위치 정보를 기반으로 초기 캔버스 범위를 계산하여 프로젝트 XML에 삽입
  connect( createdProject, &QgsProject::writeProject, [createdProject, createdProjectExtent, positionInformation]( QDomDocument &document ) {
    QDomNodeList nodes = document.elementsByTagName( "qgis" );
    if ( !nodes.isEmpty() )
    {
      QDomNode node = nodes.item( 0 );
      QDomElement element = node.toElement();

      // mapcanvas 요소 생성 및 추가
      QDomElement canvasElement = document.createElement( QStringLiteral( "mapcanvas" ) );
      canvasElement.setAttribute( QStringLiteral( "name" ), QStringLiteral( "theMapCanvas" ) );

      node.appendChild( canvasElement );

      // GPS 위치 기반으로 초기 지도 범위 계산 후 XML에 기록
      QgsRectangle extent = PositioningUtils::createExtentForDevice( positionInformation, createdProject->crs(), createdProjectExtent );
      if ( !extent.isEmpty() )
      {
        QgsMapSettings mapSettings;
        mapSettings.setDestinationCrs( createdProject->crs() );
        mapSettings.setOutputSize( QSize( 500, 500 ) );
        mapSettings.setExtent( extent );
        mapSettings.writeXml( canvasElement, document );
      }
    }
  } );

  // 프로젝트를 파일로 저장하고 메모리에서 해제
  const bool written = createdProject->write( projectFilepath );
  createdProject->clear();
  createdProject->deleteLater();

  // 이전에 저장된 프로젝트 캐시 설정 제거 (충돌 방지)
  QSettings().remove( QStringLiteral( "/qgis/projectInfo/%1" ).arg( projectFilepath ) );

  // 저장 성공 시 프로젝트 파일 경로 반환, 실패 시 빈 문자열 반환
  return written ? projectFilepath : QString();
}