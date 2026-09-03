# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

이 저장소는 [QField](https://github.com/opengisch/QField) (QGIS 기반 필드 조사용 크로스플랫폼 앱, C++/QML, GPLv2+)의 포크로, 인프라/시설물 점검(facility inspection) 용도로 커스터마이징 중이다. 업스트림 QField와 동기화되는 대규모 코드베이스이므로, 커스텀 변경은 최소한의 지점에 집중시키고 업스트림 구조를 그대로 유지하는 것이 원칙이다.

## 빌드 & 테스트 명령어

이 프로젝트는 CMake + vcpkg 기반이며, 전체 의존성 빌드는 수 시간이 걸릴 수 있다. 로컬에서 이미 구성된 빌드 디렉터리(`build/`, `builddir/` 등)가 있는지 먼저 확인하고, 없으면 새로 구성 전에 사용자에게 어떤 플랫폼/트리플렛으로 빌드할지 확인할 것.

### Windows (이 저장소의 기본 개발 환경)

Windows에서는 항상 vcpkg로 빌드한다. Visual Studio + cmake 필요.

```sh
cmake -S . -B build \
  -D VCPKG_TARGET_TRIPLET=x64-windows-static \
  -D CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" \
  -D PKG_CONFIG_EXECUTABLE=build/vcpkg_installed/x64-windows-static/tools/pkgconf/pkgconf.exe \
  -D VCPKG_INSTALL_OPTIONS="--x-buildtrees-root=C:/build" \
  -D ENABLE_TESTS=ON

cmake --build build
```

- `x-buildtrees-root`는 vcpkg 빌드 시 경로 길이 제한 문제를 피하기 위해 짧은 경로(예: `C:/build`)로 지정해야 한다.
- CI(`.github/workflows/windows.yml`)는 `clang-cl` + Ninja + `x64-windows-static`/`arm64-windows-static` 트리플렛을 사용한다. 로컬에서도 이 조합으로 재현 가능.

### Linux / macOS / Android / iOS

플랫폼별 상세 절차는 [doc/dev.md](doc/dev.md)에 정리되어 있다 (시스템 패키지 vs vcpkg 옵션, Android 트리플렛, iOS 시뮬레이터/디바이스 빌드 등). 요약:

```sh
# 시스템 패키지 사용 (Linux, 가장 빠름)
cmake -S . -B build
cmake --build build

# vcpkg로 전체 의존성 빌드 (모든 플랫폼, 매우 느림)
cmake -S . -B build -DWITH_VCPKG=ON
cmake --build build
```

Android: `triplet=arm64-android ./scripts/build.sh` (도커 이미지 사용) 또는 `scripts/build-for-linux.sh` 참고.

### 테스트

- `-DENABLE_TESTS=ON`으로 구성한 뒤 빌드 디렉터리에서 `ctest` 실행.
- 테스트 프레임워크는 Catch2 (v3 이상 필요). 개별 테스트 실행: `ctest -R <테스트이름>` (테스트 이름은 [test/CMakeLists.txt](test/CMakeLists.txt)의 `ADD_CATCH2_TEST(...)` 첫 번째 인자, 예: `featureutilstest`, `attributeformmodeltest`).
- QML 관련 테스트는 `-E qmltest`로 CI에서 제외되는 경우가 있음 (환경 의존적).
- Spix 기반 UI 자동화 테스트는 `test/spix/`에 위치, 별도 Python 요구사항(`pip install -r test/spix/requirements.txt`) 필요.

### 포맷팅 / 린트

`pre-commit`으로 관리된다 (설치: `pip install pre-commit && pre-commit install`). 훅 구성은 [.pre-commit-config.yaml](.pre-commit-config.yaml) 참고:

- C++: `clang-format` ([.clang-format](.clang-format), 2-space indent)
- QML: `qmlformat` ([.qmlformat.ini](.qmlformat.ini), 2-space indent)
- CMake: `cmake-format` ([.cmake-format.yaml](.cmake-format.yaml))
- Shell: `shfmt` + `shellcheck`
- Python: `black`

C++ 코드에서 사용이 금지된 키워드/패턴이 있다 (`scripts/test_banned_keywords.sh`가 CI에서 검사): `DBL_MAX`/`INT_MIN` 등 대신 `std::numeric_limits<T>`, `qMin`/`qMax`/`qAbs`/`qRound`/`qSort` 등 대신 STL 대응 함수(`std::min`, `std::fabs`, `std::round`, `std::sort`...), `QScopedPointer`/`QSharedPointer` 대신 `std::unique_ptr`/`std::shared_ptr`, Doxygen 주석은 `@param`이 아닌 `\param` 스타일. 예외가 필요하면 해당 라인에 `// skip-keyword-check` 주석을 단다.

## 아키텍처 개요

### 큰 그림: C++ 코어 + QML UI

- [src/app/](src/app/) — 애플리케이션 진입점 (`main.cpp`). `QgsApplication` 초기화, 번역기(`TranslatorManager`), 플랫폼별 초기화, Sentry(옵션), QML 엔진 부트스트랩을 담당.
- [src/core/](src/core/) — C++ 비즈니스 로직 전체. QML에 노출되는 모델/컨트롤러/유틸리티 클래스들이 여기 있다. 하위 폴더 구조:
  - `utils/` — 프로젝트·레이어·지오메트리·표현식 등 도메인 유틸리티. 정적 메서드 위주의 `XxxUtils` 클래스 패턴 (`ProjectUtils`, `LayerUtils`, `FeatureUtils`, `GeometryUtils` 등).
  - `qgsquick/` — QGIS(QGIS Quick) 지도 렌더링을 QML `MapCanvas`에 연결하는 브리지 클래스.
  - `locator/` — 검색/로케이터 필터 구현체 (지도 검색창).
  - `cogo/` — COGO(좌표 기하 계산) 연산.
  - `3d/` — 3D 지형/렌더링 관련 클래스.
  - `qgismobileapp.{h,cpp}` — 앱 전체를 아우르는 최상위 컨트롤러(대부분의 QML 최상위 프로퍼티/시그널이 여기서 노출됨). 새 전역 기능을 추가할 때 우선 확인할 파일.
- [src/qml/](src/qml/) — UI 전체. `MapCanvas.qml`, `FeatureForm.qml`, `DashBoard.qml` 등 화면/컴포넌트 단위 `.qml` 파일. C++ 쪽에서 `Q_PROPERTY`/`Q_INVOKABLE`로 노출한 모델·유틸을 QML에서 바인딩해 사용하는 구조.
- [src/service/](src/service/) — QFieldCloud 동기화(`qfieldcloudservice`), GNSS 측위(`qfieldpositioningservice`) 등 백그라운드/플랫폼 서비스.
- [platform/](platform/) — OS별 패키징/배포 리소스 (android/ios/linux/macos/windows), 앱 매니페스트, 아이콘 등.

빌드는 각 디렉터리의 `CMakeLists.txt`가 소스 목록을 관리하며 ([src/core/CMakeLists.txt](src/core/CMakeLists.txt)가 가장 크다), 새 `.cpp`/`.h`를 추가하면 해당 `CMakeLists.txt`의 소스 목록에도 등록해야 빌드에 포함된다.

### 프로젝트 생성 흐름과 이 포크의 커스터마이징 지점

`ProjectUtils::createProject()` ([src/core/utils/projectutils.cpp](src/core/utils/projectutils.cpp))가 QField의 "새 프로젝트 만들기" 흐름 전체를 담당한다: GeoPackage 레이어(메모/트랙) 생성 → 필드 정의 → 위젯(`QgsEditorWidgetSetup`) 및 별칭 설정 → 속성 폼 탭 레이아웃 구성 → 관계(relation) 등록 → 배경지도 추가 → 프로젝트 파일(`.qgz`) 저장까지 한 함수에서 처리한다.

이 포크의 시설물 점검 커스터마이징은 대부분 이 함수 안에 집중되어 있다:
- 메모(Notes) 레이어 필드가 시설물 속성(`fclt_nm` 시설명, `inst_nm` 기관명, `lotno_addr`/`daddr` 주소, `pic_*` 담당자 정보, `facility_condition` 시설물 상태, `repair_required_yn` 보수 필요 여부, `facility_memo` 특이사항, `photo_1`~`photo_5`, `audio_memo`, `video`)으로 확장되어 있다.
- 속성 폼은 드래그앤드롭 레이아웃(`Qgis::AttributeFormLayout::DragAndDrop`)으로 재구성되어 3개 탭으로 구성된다: **기본 정보**(시설명/기관명/주소/담당자) → **시설물 관리**(상태/보수 여부/특이사항/점검일시) → **현장 미디어**(사진 5장/오디오/비디오 + 첨부파일 관계 위젯).
- 미디어 필드는 `ExternalResource` 위젯의 `DocumentViewer` 옵션으로 타입을 구분한다: `1`=이미지, `3`=오디오, `4`=비디오. 아이콘/뷰어가 이 값으로 결정되므로 필드를 추가/변경할 때 반드시 맞는 값을 지정해야 한다.
- 트랙(Tracks) 레이어에도 동일한 시설물 속성 필드 세트가 동기화되어 있다 (필드 추가 시 두 레이어 모두 갱신 필요).

**이 함수를 수정할 때 주의할 점:**
1. 필드를 추가/이름 변경할 때는 (a) `QgsFields` 정의, (b) `QgsEditorWidgetSetup` + `setFieldAlias`, (c) 해당 탭의 `basicFields`/`facilityFields`/`mediaFields` 문자열 리스트 세 곳을 모두 갱신해야 UI에 반영된다.
2. 메모 레이어와 트랙 레이어는 별도의 `QgsFields`/위젯 설정 블록을 갖고 있어 필드 세트가 중복 정의된다. 시설물 속성을 바꿀 때 두 블록을 함께 확인할 것.
3. 좌표계는 저장 시 `EPSG:3857`, 표시 시 `EPSG:4326`을 쓰며, 지오메트리는 3D(`PointZ`)가 아닌 2D(`Point`)로 강제되어 있다 (최근 수정 사항). 좌표/지오메트리 타입을 건드릴 때 이 제약을 유지할 것.
4. 이 파일은 업스트림 QField의 파일이므로, 향후 업스트림과 병합(rebase/merge)할 가능성을 고려해 변경 범위를 가능한 한 좁게 유지하고 원본 로직(트랙/메모 공통 처리, 배경지도 처리 등)은 건드리지 않는 것이 안전하다.

### 국제화

`src/app/main.cpp`의 `TranslatorManager`가 `i18n/*.ts` 파일을 로드한다. 한국어를 포함한 다국어 UI 문자열은 `tr()`로 감싸며, Transifex로 관리되는 업스트림 번역과 별개로 이 포크의 시설물 관련 문자열(예: "시설명", "시설물 상태")은 소스 코드에 하드코딩된 한국어 `tr()` 문자열로 존재한다.

## 참고

### 리포지토리 구조 추가 정보

- [test/](test/) — Catch2 단위 테스트(`test_*.cpp`), QML 테스트(`test/qml/`), Spix UI 자동화(`test/spix/`), NMEA/PostGIS/QGIS 서버 도커 기반 통합 테스트 환경(`test/nmea_server/`, `test/postgis_docker/`, `test/qgis_server_docker/`).
- [scripts/](scripts/) — 빌드 스크립트(`build.sh`, `build-for-linux.sh`, `build-vcpkg.sh`), CI 스크립트(`scripts/ci/`), 정적 분석(`cppcheck.sh`), 라이선스 검사(`test_licenses.sh`, `licensecheck.pl`), 금지 키워드 검사(`test_banned_keywords.sh`), 버전 관리(`version_number.sh`).
- [resources/](resources/) — 폰트, 사운드, 3D 에셋, `sample_projects/`(샘플 QGIS 프로젝트), `theme/`(테마 JSON, `APP_THEME_PATH`로 커스텀 가능).
- [.devcontainer/](.devcontainer/)와 [.docker/](.docker/) — Ubuntu 22.04 기반 개발 컨테이너 (CLion/VS Code Dev Containers 지원). GUI 실행 시 X11 문제가 있으면 호스트에서 `xhost +local:` 필요.

### CI 워크플로우

[.github/workflows/](.github/workflows/)에 플랫폼별 빌드(`windows.yml`, `linux.yml`, `macos.yml`, `android.yml`, `ios.yml`), 포맷 검사(`pre-commit.yml`), 스크립트 검사(`script_checks.yml`, 버전 번호 검증), CodeQL 정적 분석(`codeql.yml`), 번역 동기화(`translations.yml`, `sync-translations.yml`)가 정의되어 있다. PR에서 포맷 문제가 발생하면 `@qfield-fairy style please` 코멘트로 자동 수정 트리거 가능.

### 기여 규칙

- 새 기능을 커밋할 때는 커밋 메시지에 `[FEATURE]` 태그와 명확한 설명을 포함 (이 포크에서는 `[fix]`/`[feat]` 접두사 관례가 사용되고 있다).
- Pull request는 업스트림 QField 저장소 기준 (`CONTRIBUTING.md` 참고). 이 포크에서 별도 PR 정책이 없다면 커밋 메시지 태깅 관례만 따르면 된다.
