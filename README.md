# Naru Studio

**C++ 자체 게임 엔진 + Qt 제작 UI + Python 게임 확장 스크립트**를 만드는 프로젝트입니다.

RPG Maker의 데이터베이스·맵·이벤트·전투 등 제작 기능을 출발점으로 삼고, Unity·Godot의 유용한 기능/작업 방식을 참고해 UI로 쉽게 제작할 수 있도록 합니다. 지원 목표는 턴제 RPG와 2D 횡스크롤입니다. 기존 엔진을 감싼 편집기가 목표가 아닙니다.

## 지금 시작할 곳

1. **[다음 세션 구축 안내](NARU_NEXT_STEPS.md)** — 확정한 목표, 완료/미완료, 설치 준비, 소스 구조, 단계별 구현과 검증 기준.
2. [세션 기록](SESSION_STATE.md), [작업 안내](AGENTS.md).
3. [기능·UI 설계 초안](TOOL_FOUNDATION.md). 최상단의 최신 C++/Qt/Python 갱신을 우선합니다.

## 현재 상태

**C++/Qt/Python 제품 구현은 아직 시작 전입니다.** 조사 자료와 과거 Godot 실험본을 이관하고 새 엔진 구축 계획을 정리한 상태입니다. 새 엔진의 설치 파일이나 실행 파일이 이미 완성된 것은 아닙니다.

| 위치 | 내용 |
|---|---|
| `analysis/rpgmaker/` | RPG Maker 과거 분석 01~07, MV 코어 4파일 REA 재조사 08, 요약/원시 Evidence |
| `analysis/godot/` | Godot 소스 관측·실행 검증·REA export·Ghidra 콘솔 래퍼 분석 |
| `analysis/unity/` | 설치 확인·문서 비교, REA 메타데이터/7타입 관측과 인스펙터·직렬화·Undo·프리팹 공개 소스 조사 |
| `analysis/rea-connection/` | REA 설치·MCP 연결·Ghidra 검증 근거 |
| `analysis/foundation/` | 파일 해시·행 번호를 포함한 소스 관측 |
| `refs/` | MV 공개 코어·mkxp·Godot 핵심 소스 일부와 각각의 라이선스 |
| `probes/`, `tools/` | 조사와 검증에 사용한 코드 |
| `app/` | **최종 제품이 아닌 Godot/GDScript 실험본**과 통합 검증 코드 |

세 제품 전체를 전수 분석한 상태는 아닙니다. 문서/소스 조사와 제한된 도구·실행 검증의 범위는 각 조사 README를 참고하세요.

## 다른 컴퓨터에서 이어가기

```powershell
git clone https://github.com/Aristia0930/Naru_Studio.git
cd Naru_Studio
```

이후 `NARU_NEXT_STEPS.md`를 읽고 개발 도구부터 준비합니다. C++ 컴파일러·Qt kit·CMake·Python 임베딩 버전과 빌드 설정은 다음 작업에서 검증하고 고정합니다. 현재 문서에 없는 빌드 명령을 동작이 검증된 것처럼 안내하지 않습니다. 개발 도구와 SDK를 Git에 묶어 올리는 대신 설치 방법/버전을 기록합니다.

Godot·Ghidra·REA는 이전 조사 도구이며 새 엔진의 필수 실행 환경이 아닙니다. 참고 자료를 조사할 때만 별도로 준비합니다. PC별 경로·캐시·자격증명을 공유하지 않습니다.

## 이전 실험본을 실행하려면

`app/`은 39개 통합 검증을 통과한 UI/행동 참고용 Godot 프로젝트입니다. 장면·이벤트·코드 연결·즉시 실행·로그·저장·음원/이동 연출을 실험했습니다. 제품 구현을 여기에 이어 붙이지 않습니다.

Godot 4.7.2 standard Windows x64를 [공식 배포처](https://github.com/godotengine/godot-builds/releases/tag/4.7.2-stable)에서 받아 `refs/godot-4.7.2/bin/`에 압축을 풀면 기존 `Start Studio.cmd`로 실행할 수 있습니다. 런처 이름과 화면의 과거 Studio 표기는 실험본에 남아 있습니다. 엔진 바이너리·ZIP·캐시·테스트 산출물은 Git에서 제외합니다.

실험본 검증: Godot 콘솔 실행 파일에 `--headless --path app --script tests/integration.gd --log-file <절대경로>`를 전달합니다. 정확한 로컬 경로를 바꾸어 사용하세요. 실험본의 `.gd` 확장은 최종 엔진의 Python 스크립트 API가 아닙니다.

## 참고 소스

외부 참고 소스의 LICENSE/COPYING과 provenance를 유지합니다. 일부 `refs`는 다른 라이선스의 조사용 소스이므로 제품 코드로 복사하지 않습니다. 원시 Evidence의 절대 경로는 조사 당시 위치의 기록이며, 그 경로가 새 PC에 존재해야 한다는 뜻이 아닙니다.
