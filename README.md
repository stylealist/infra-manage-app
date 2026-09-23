<!-- ===== SJ-LAB 포크 안내 (아래 본문은 업스트림 QField 문서 원본) ===== -->

# infra-manage-app — 시설물 점검용 QField 포크

> [QField](https://github.com/opengisch/QField)(QGIS 기반 현장조사 앱, C++/QML)의 **포크**로, 인프라·시설물 점검 용도로 커스터마이징한 앱입니다.
> sj-lab 시설물 관리 플랫폼에서 **데이터가 시작되는 지점**입니다.

```
[이 앱] 현장에서 시설물 점검·사진·음성 메모 입력
   │ 업로드
[QFieldCloud] https://qfield.sj-lab.co.kr
   │ 30초 주기 동기화(sj-qfieldsync)
[PostGIS] qfield 스키마 → [백엔드 mapservice-rest] → [웹 지도 sj-lab.co.kr/map/]
```

- **기본 브랜치는 `master`** 입니다(다른 sj-lab 저장소는 `main`).
- 업스트림과 계속 동기화되는 대규모 코드베이스이므로, **커스텀 변경은 최소 지점에 집중하고 업스트림 구조를 그대로 유지**하는 것이 원칙입니다.
- 빌드는 CMake + vcpkg 기반이며 전체 의존성 빌드에 수 시간이 걸립니다. 기존 빌드 디렉터리와 대상 플랫폼을 먼저 확인하세요. 상세 절차는 아래 업스트림 문서와 `doc/dev.md`, 작업 규칙은 `CLAUDE.md`를 참고합니다.
- 수집된 데이터가 어떻게 흘러 웹 지도까지 가는지는 총괄 저장소 `mapservice-rest`의 `docs/system-architecture.md`에 정리돼 있습니다.

---
[![Read the Docs](https://img.shields.io/badge/Read-the%20Docs-green.svg)](https://docs.qfield.org/)
[![Community Platform](https://img.shields.io/discourse/topics?server=https://community.qfield.org)](https://community.qfield.org)
[![Sponsor](https://img.shields.io/static/v1?label=Support&message=%E2%9D%A4)](https://github.com/sponsors/opengisch)
[![Contribute](https://img.shields.io/static/v1?label=Contribute&message=💪)](#contribute)
[![Release](https://img.shields.io/github/release/opengisch/QField.svg?label=Release)](https://github.com/opengisch/QField/releases)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/8392/badge)](https://www.bestpractices.dev/projects/8392)
[![Digital Public Good](https://img.shields.io/badge/Digital%20Public%20Good-verified-brightgreen)](https://www.digitalpublicgoods.net/r/qfield)
[![QFieldCloud](https://img.shields.io/badge/QFieldCloud-GitHub-blue)](https://github.com/opengisch/QFieldCloud)

# QField for QGIS  

A simplified touch-optimized interface for QGIS in the field.

[![Visit QField's homepage](https://github.com/user-attachments/assets/88771ae0-3701-4cf4-8d8c-cd295c0831b1)](https://qfield.org)

## 🧭 About QField

QField works fully offline or connected, and supports seamless synchronization with the optional [**QFieldCloud** platform](https://qfield.cloud) for collaborative field-to-office workflows.
You can find the open-source QFieldCloud backend on GitHub here: [github.com/opengisch/QFieldCloud](https://github.com/opengisch/QFieldCloud)

QField is officially recognized as a [Digital Public Good](https://digitalpublicgoods.net/r/qfield) for its contributions to open, inclusive, and sustainable digital development.

Explore the full documentation at [docs.qfield.org](https://docs.qfield.org/)

## 📲 Get QField
<p align="center">
  <a href="https://play.google.com/store/apps/details?id=ch.opengis.qfield"><img src="https://qfield.org/images/play_store.png" alt="Get it on Google Play" height="60"/></a>
  <a href="https://apps.microsoft.com/detail/xp99h3bcx4bw7f"><img src="https://qfield.org/images/download_windows.png" alt="Get it on Microsoft Store" height="60"/></a>
  <a href="https://apps.apple.com/app/qfield-for-qgis/id1531726814"><img src="https://qfield.org/images/app_store.png" alt="Get it on the App Store" height="60"/></a>
</p>
<p align="center">
  <a href="https://qfield.org/get-latest?platform=linux"><img src="https://cdn.jsdelivr.net/gh/devicons/devicon/icons/linux/linux-original.svg" alt="Linux" width="20"/>Download for Linux</a>
  <a href="https://qfield.org/get-latest?platform=macos"><img src="https://cdn.jsdelivr.net/gh/devicons/devicon/icons/apple/apple-original.svg" alt="macOS" width="20"/>Download for macOS</a>
</p>

### All Platforms
📦 Prefer direct downloads or older versions?  Check out the full list of releases on [GitHub Releases](https://github.com/opengisch/QField/releases)

### Get master (unstable) version

We automatically publish the latest master build to a [dedicated channel on the playstore](https://play.google.com/store/apps/details?id=ch.opengis.qfield_dev). You'll need to [join the beta program](https://play.google.com/apps/testing/ch.opengis.qfield_dev) to start getting the latest version.

Please remember that this is the latest development build and is not meant for production.

## Contribute

QField is an open source project, licensed under the terms of the GPLv2 or later. This means that it is free to use and modify and will stay like that.

We are very happy if this app helps you to get your job done or in whatever creative way you may use it.

If you found it useful, we will be even happier if you could give something back. A couple of things you can do are

 * Rate the app [★★★★★](https://play.google.com/store/apps/details?id=ch.opengis.qfield&hl=en#details-reviews)
 * Write about your experience (please [let us know](mailto:sales@qfield.cloud)!)
 * [Help with the documentation](https://github.com/opengisch/QField-docs#documentation-process)
 * [Translate the documentation](https://github.com/opengisch/QField-docs#translation-process) or [the app](https://explore.transifex.com/opengisch/qfield-for-qgis/)
 * [Sponsor a feature](https://qfield.org/support-us/)
 * And just drop by to say thank you or have a beer with us next time you meet OPENGIS.ch at a conference

## Share

The world loves to hear about the usage of QField, follow us or share your story on your favorite channel

[![share on linkedin](images/icons/linkedin.svg)](https://www.linkedin.com/products/opengisch-qfield/)
[![share on bluesky](images/icons/bluesky.svg)](https://bsky.app/profile/qfield.bsky.social/share?text=Looking%20for%20a%20good%20tool%20for%20field%20work%20in%20GIS?%20Check%20out%20%23QField!)
[![share on mastodon](images/icons/mastodon.svg)](https://mastodon.social/share?text=Looking%20for%20a%20good%20tool%20for%20field%20work%20in%20GIS?%20Check%20out%20%23QField!)
[![share on X](images/icons/twitter-x.svg)](https://x.com/QFieldForQGIS)

## Development

For development information, refer to the dedicated [developer documentation](doc/dev.md).

## Verify Authenticity of the App packages (Android only)

SHA-256 hash of signing certificate:

```5a7dd946a4b700c081a5bd375dbc8f0d11aa89d53832567ce5b8a92088e0e898```

Use the following command to verify the hash of the signing certificate:

```apksigner verify --print-certs [filename.apk] | grep "5a7dd946a4b700c081a5bd375dbc8f0d11aa89d53832567ce5b8a92088e0e898"```
