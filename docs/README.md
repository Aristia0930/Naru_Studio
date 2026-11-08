# Naru Studio 문서 위치 안내

현재 사용법은 아래 가이드와 API 문서를 기준으로 합니다. 개발 계획의 미구현 항목과 과거 세션 기록은 현재 기능 목록과 구분합니다.

## 게임 제작·테스트

| 문서 | 내용 |
|---|---|
| [사용 설명서](guide/README.md) | 프로젝트 생성부터 장면·이벤트·게임 실행까지 기본 흐름 |
| [이벤트 명령](guide/EVENTS.md) | 명령 14종의 입력·실행·대기·중지·F6와 게임의 차이 |
| [에셋 관리](guide/ASSETS.md) | 가져오기·그룹·ID 변경·사용처·교체·내보내기 |
| [객체 이미지·배경](guide/IMAGES.md) | 그림과 충돌 영역, 표시 설정, Python 변경 |
| [BGM·효과음](guide/AUDIO.md) · [단계별 소리 테스트](guide/AUDIO_TEST.md) | WAV/MP3·이벤트·Python 연결, 청취 확인 순서 |
| [객체 이름표](guide/NAMEPLATES.md) | F9 전체 이름/ID, 개별 이름·배경 투명/색·위/아래 |
| [키 설정](guide/INPUT_SETTINGS.md) | 화면과 전용 Python 설정, 동시 입력 |
| [실행 중 확인 도구](guide/DEBUGGING.md) | 변수 확인·오류 위치 이동·파일 로그 |
| [단축키](SHORTCUTS.md) | 제작툴과 게임 창의 키 동작 |

## Python·실행 예제

| 문서·폴더 | 내용 |
|---|---|
| [스크립트 연결](guide/SCRIPTING.md) | 파일 작성·F5/F7·이벤트 호출·전역 등록 |
| [Python 기능별 가이드](guide/PYTHON_COOKBOOK.md) | 복사 가능한 코드·연결 순서·오류·현재 미지원 범위 |
| [스크립트 API](SCRIPT_API.md) | `ctx.*` 값·함수·인자·반환·실행 시점 |
| [예제 목록](examples/README.md) | 단일 Python 파일과 열어서 실행하는 프로젝트 |
| [가이드 그림](guide/images/) | 툴·player가 촬영한 PNG. 재촬영 명령은 각 가이드에 기재 |

## 빌드·개발 재개

| 문서 | 내용 |
|---|---|
| [빌드·설치·검사](BUILD.md) | 다른 PC의 의존성 설치·CMake·`dev check` |
| [로드맵](ROADMAP.md) | 구현 순서·현재 완료 범위·다음 작업 |
| [세션 기록](../SESSION_STATE.md) | 최신 작업·검증·보존한 사용자 변경. 최신 항목부터 확인 |
| [상세 개발 계획](DEVELOPMENT_PLAN.md) | 공통 2D 기반과 RPG·횡스크롤 기능, 게임 완주 검증 기준 |
| [DB·에셋 확장 계획](DATABASE_ASSETS_PLAN.md) | 현재 구현과 앞으로 개발할 DB·에셋·외부 연결 범위 |

저장소 입구는 [루트 README](../README.md)입니다. 개발 재개 시 **로드맵 → 세션 기록 → 루트 README** 순서로 읽고 실제 파일 상태를 확인합니다. 루트의 `NARU_NEXT_STEPS.md` 등 초기 설계 문서는 이전 결정의 배경 자료이며 최신 사용자 지시와 현재 로드맵이 우선합니다.
