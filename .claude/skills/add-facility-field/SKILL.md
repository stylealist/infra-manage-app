---
name: add-facility-field
description: 시설물 점검 프로젝트 생성 로직(ProjectUtils::createProject, src/core/utils/projectutils.cpp)에 새 시설물 속성 필드를 추가하거나, 기존 필드 이름을 바꾸거나, 필드를 제거할 때 사용한다. "시설물 필드 추가해줘", "새 컬럼 추가", "이 필드를 폼 탭에 넣어줘", "필드 이름 바꿔줘" 같은 요청에 사용.
---

# 시설물 필드 추가/수정 워크플로우

이 저장소(QField 포크)에서 시설물 점검용 속성 필드는 [src/core/utils/projectutils.cpp](../../../src/core/utils/projectutils.cpp)의 `ProjectUtils::createProject()` 함수 안에서 정의된다. 이 함수는 새 프로젝트를 만들 때 GeoPackage 레이어(메모/트랙)의 필드, 위젯, 폼 탭 레이아웃을 코드로 직접 구성한다. 필드 하나를 추가/변경하려면 **여러 곳을 동시에** 고쳐야 UI에 정상 반영된다. 한 곳이라도 빠뜨리면 "필드는 있는데 폼에 안 보임" 같은 조용한 버그가 생긴다.

## 전제: 메모(Notes) 레이어와 트랙(Tracks) 레이어는 별개다

`createProject()` 안에는 두 개의 독립된 블록이 있다:
- **메모 레이어 블록** (`options.value("notes")` 조건 안, `notesLayer` 변수) — 시설물 점검 데이터를 담는 주 레이어. 사진/오디오/비디오, 시설물 상태 등 대부분의 시설물 속성이 여기 있다.
- **트랙 레이어 블록** (`options.value("tracks")` 조건 안, `tracksLayer` 변수) — 이동 경로를 기록하는 레이어. 시설물 식별/담당자 정보 위주로 메모 레이어와 겹치는 필드 세트를 별도로 정의하고 있다.

**두 블록은 `QgsFields`/`QgsEditorWidgetSetup` 코드가 완전히 중복되어 있다.** 새 필드가 두 레이어 모두에 필요한지, 메모 레이어에만 필요한지(예: 사진/오디오/비디오는 트랙 레이어에 없음) 먼저 판단하고, 필요한 만큼만 각 블록에 반영한다.

## 필드 하나를 추가할 때 고쳐야 할 지점 (메모 레이어 기준)

1. **필드 정의** — `QgsFields fields;` 블록에 `fields.append( QgsField( QStringLiteral( "필드명" ), QMetaType::QString ) );` 형태로 추가. 타입은 `QMetaType::QString`/`QDateTime` 등 기존 필드를 참고.
2. **위젯 설정 + 별칭** — `fieldIndex = fields.indexOf( QStringLiteral( "필드명" ) );` 로 인덱스를 찾고, `if ( fieldIndex >= 0 )` 안에서 `QgsEditorWidgetSetup`을 만들어 `notesLayer->setEditorWidgetSetup(...)`, 한글 표시명은 `notesLayer->setFieldAlias( fieldIndex, tr( "표시명" ) );`로 설정. 위젯 타입은 필드 성격에 따라 선택:
   - 단순 텍스트: `"TextEdit"`
   - 날짜/시간: `"DateTime"` (기존 `inspected_at` 필드 설정을 그대로 참고)
   - 선택지(드롭다운): `"ValueMap"` — `widgetOptions["map"]`에 `QVariantMap`으로 표시명→저장값 매핑 (예: `facility_condition`, `repair_required_yn` 참고)
   - 사진/파일: `"ExternalResource"` — 아래 "미디어 필드" 절 참고
3. **폼 탭 필드 리스트** — 이 필드가 어느 탭(`기본 정보`/`시설물 관리`/`현장 미디어`)에 보여야 하는지 정하고, 해당 `basicFields`/`facilityFields`/`mediaFields` `QStringList`에 필드명을 추가. **이 리스트에 없으면 필드가 존재해도 폼에 나타나지 않는다.**
4. **트랙 레이어에도 필요한가?** — 필요하면 트랙 레이어 블록에서 1~2를 동일하게 반복한다 (트랙 레이어는 탭 구조가 없고 필드 순서대로 나열되므로 3은 해당 없음).

## 미디어 필드(사진/오디오/비디오)를 추가할 때

`ExternalResource` 위젯의 `DocumentViewer` 옵션 값으로 뷰어 타입이 결정된다. 값이 틀리면 아이콘/미리보기가 깨지므로 정확히 지정한다:

| 미디어 타입 | DocumentViewer 값 | 참고 필드 |
|---|---|---|
| 이미지 | `1` | `photo_1` ~ `photo_5` |
| 오디오 | `3` | `audio_memo` |
| 비디오 | `4` | `video` |

사진을 하나 더 추가한다면(`photo_6` 등) 기존 `photo_5` 블록을 그대로 복사해 필드명과 별칭만 바꾸면 된다. 오디오/비디오는 `FileWidgetFilter`로 허용 확장자를 제한하고 있으니 기존 값(`audio_memo`, `video`)을 참고해 확장자 목록을 유지한다.

## 좌표계/지오메트리 제약 (건드리지 말 것)

메모 레이어는 `Qgis::WkbType::Point` (2D, EPSG:4326 저장 후 EPSG:3857로 프로젝트 좌표계 설정), 트랙 레이어는 `LineStringZM`으로 고정되어 있다. 필드 추가 작업 중에는 이 지오메트리/좌표계 설정을 바꾸지 않는다 — 과거 3D(`PointZ`)에서 2D(`Point`)로 되돌린 이력이 있다(커밋 `095ffb495`).

## 완료 후 확인

1. 필드명을 새로 추가/변경했다면, 같은 이름을 3곳(필드 정의, 위젯 설정, 탭 리스트) 모두에서 검색해서 오타 없이 일치하는지 확인한다: `git grep -n "필드명" src/core/utils/projectutils.cpp`
2. 사용자 노출 별칭 문자열이 `tr()`로 감싸져 있는지 확인한다 (이 포크의 관례상 한국어 리터럴을 직접 사용해도 무방하지만 `tr()` 래핑은 유지).
3. `scripts/test_banned_keywords.sh`가 걸리는 패턴(특히 `QStringLiteral( "" )` 같은 빈 문자열 리터럴)을 새로 추가하지 않았는지 확인한다.
4. 가능하면 로컬 빌드(`cmake --build build`)로 컴파일 오류가 없는지 확인한다. `ProjectUtils::createProject`는 코드가 매우 길어 괄호/블록 스코프 실수가 나기 쉽다.
