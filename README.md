# infra-manage-app — 시설물 점검 모바일 현장조사 애플리케이션 (QField Fork)

`infra-manage-app`은 오픈소스 GIS 현장조사 플랫폼 [QField](https://github.com/opengisch/QField)를 기반으로 인프라 및 공공 시설물 점검(Facility Inspection) 업무에 맞추어 특화 개발된 크로스플랫폼 모바일 애플리케이션(C++/QML)입니다. sj-lab 시설물 관리 플랫폼에서 **현장 데이터가 생성되는 최초 시작점**입니다.

---

## 1. 애플리케이션 역할 및 핵심 책임

- **현장 공간정보 및 속성 수집**: 고정밀 GNSS 측위 또는 수동 지정을 통해 현장 시설물의 2D 지리좌표(Point)와 상태 정보를 취득합니다.
- **맞춤형 3단계 점검 속성 폼**: 기본 정보(시설명/기관명/주소/담당자), 시설물 관리(상태/보수필요여부/특이사항), 현장 미디어(사진 5장/음성메모/동영상)로 세분화된 현장 맞춤형 UI 제공.
- **클라우드 실시간 동기화**: 오프라인 환경에서 수집된 공간 데이터와 첨부 미디어를 QFieldCloud(`https://qfield.sj-lab.co.kr`)로 자동 패키징 업로드.

---

## 2. 기술 스택

- **언어 및 프레임워크**: C++17, Qt (QML, QtQuick), QGIS Core SDK
- **빌드 시스템 및 패키징**: CMake, vcpkg, Clang / MSVC
- **데이터 포맷**: GeoPackage (OGC GPKG, SQLite 기반 벡터/속성 저장)
- **외부 연동**: QFieldCloud REST API, NMEA GNSS 리시버

---

## 3. 현장 데이터 파이프라인 및 프로세스

```
[현장 점검자]
       │
       ▼ 현장조사 앱 (infra-manage-app)
  ├── 1. 모바일 앱에서 시설물 위치 터치 / GNSS 측정
  ├── 2. 점검 폼 입력:
  │      ├─ 기본 정보 (시설명, 기관명, 지번/도로명 주소, 담당자)
  │      ├─ 시설물 관리 (상태 등급, 보수 필요 여부 'Y'/'N', 메모)
  │      └─ 현장 미디어 (사진 최대 5장, 음성 메모, 점검 동영상)
  └── 3. GeoPackage 로컬 저장 (EPSG:3857 점 좌표)
       │
       ▼ 업로드 (QFieldCloud 동기화)
[QFieldCloud] (https://qfield.sj-lab.co.kr)
       │
       ▼ 30초 주기 동기화 (sj-qfieldsync 워커)
[PostGIS DB] ──> [mapservice-rest] ──> [웹 지도 sj-lab.co.kr/map/]
```

---

## 4. 핵심 엔지니어링 및 커스터마이징 상세

### 4.1 프로젝트 자동 구성 (`ProjectUtils::createProject`)
업스트림 QField와의 호환성을 유지하면서 시설물 점검 전용 프로젝트가 자동으로 생성되도록 비즈니스 로직을 커스터마이징했습니다:
- **속성 폼 레이아웃 재구성**: 드래그앤드롭 폼 레이아웃(`Qgis::AttributeFormLayout::DragAndDrop`)을 적용하여 3개 탭(**기본 정보**, **시설물 관리**, **현장 미디어**)을 동적으로 빌드.
- **미디어 위젯 매핑**: `ExternalResource` 위젯의 `DocumentViewer` 타입을 이미지(`1`), 오디오(`3`), 비디오(`4`)로 명확히 분기하여 현장 조사자가 앱 내에서 음성을 녹음하고 카메라 촬영을 직관적으로 연동할 수 있도록 구현.
- **2D 좌표 강제**: 3D 측정(`PointZ`) 시 발생할 수 있는 PostGIS 연산 오류를 방지하기 위해 지오메트리를 2D(`Point`, EPSG:3857)로 정규화.

### 4.2 업스트림 동기화 원칙
- 업스트림(opengisch/QField)과의 지속적인 리베이스(rebase) 및 병합을 지원하기 위해, 커스텀 변경점은 `src/core/utils/projectutils.cpp` 및 점검 모델로 최소화하여 격리 관리합니다.
- 기본 개발 브랜치는 `master`를 유지합니다.

---

## 5. 빌드 및 개발 환경 가이드

프로젝트는 CMake 및 vcpkg를 사용하여 빌드됩니다. 전체 의존성 빌드는 수 시간이 소요될 수 있으므로 기존 빌드 캐시를 확인하십시오.

### Windows 개발 환경 빌드 (MSVC / Clang)
```sh
cmake -S . -B build \
  -D VCPKG_TARGET_TRIPLET=x64-windows-static \
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" \
  -D PKG_CONFIG_EXECUTABLE=build/vcpkg_installed/x64-windows-static/tools/pkgconf/pkgconf.exe \
  -D VCPKG_INSTALL_OPTIONS="--x-buildtrees-root=C:/build" \
  -D ENABLE_TESTS=ON

cmake --build build
```

---

<!-- ===== 아래 본문은 업스트림 QField 오픈소스 프로젝트 원본 문서입니다 ===== -->

# QField for QGIS  

A simplified touch-optimized interface for QGIS in the field.

[![Visit QField's homepage](https://github.com/user-attachments/assets/88771ae0-3701-4cf4-8d8c-cd295c0831b1)](https://qfield.org)

## 🧭 About QField

QField works fully offline or connected, and supports seamless synchronization with the optional [**QFieldCloud** platform](https://qfield.cloud) for collaborative field-to-office workflows.
You can find the open-source QFieldCloud backend on GitHub here: [github.com/opengisch/QFieldCloud](https://github.com/opengisch/QFieldCloud)

QField is officially recognized as a [Digital Public Good](https://digitalpublicgoods.net/r/qfield) for its contributions to open, inclusive, and sustainable digital development.

Explore the full documentation at [docs.qfield.org](https://docs.qfield.org/)
