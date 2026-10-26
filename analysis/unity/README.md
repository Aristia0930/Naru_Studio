# 설치된 Unity 조사 — 편집 방식 비교

**2026-10-27 후속 갱신:** [REA + 공개 소스 조사](02_REA_및_공개소스_조사.md)를 추가했다. 설치 UnityEditor.dll의 아티팩트 메타데이터와 7개 타입을 REA로 관측했고, 인스펙터·직렬화·Undo·프리팹·플레이 상태의 공개 C# 참고 소스를 읽었다. members 상세는 64MiB 한도로 partial이며 공개 소스는 설치본과 다른 리비전이다. 아래 '바이너리 관측을 하지 않음'은 이전 작업 당시 설명이다. 전체 조사 완료가 아니다.

2026-10-26. Unity를 다시 다운로드하지 않고 사용자가 설치한 버전을 확인했다.

## 로컬에서 확인한 사실

[로컬 설치 관측 JSON](local-installation.json)에 경로·파일 버전·모듈 디렉터리와 확인 방법을 저장했다.

- 실행 파일: `C:\Program Files\Unity\Hub\Editor\6000.6.5f1\Editor\Unity.exe`
- Windows 파일 ProductVersion: `6000.6.5f1_3ff58d469c8a`.
- `Editor/Data/PlaybackEngines/`에 `AndroidPlayer`, `WebGLSupport`, `windowsstandalonesupport` 폴더 존재.
- 폴더 존재는 SDK·템플릿·라이선스·서명·실제 빌드 성공을 증명하지 않는다.
- 이번에는 Unity를 실행하거나 새 Unity 프로젝트를 만들지 않았다. 바이너리 디컴파일이나 내부 구현 추정도 하지 않았다.

## 공식 6.6 문서와 설계 연결

| 공식 문서에서 확인 | 우리 제작툴에 적용할 제안 |
|---|---|
| [Inspector](https://docs.unity3d.com/6000.6/Documentation/Manual/UsingTheInspector.html): 선택한 오브젝트·에셋·컴포넌트 속성 보기/편집 | 왼쪽 목록·캔버스에서 선택하면 오른쪽 속성 폼 갱신; 초보자용 필드명·설명 제공 |
| [Prefabs](https://docs.unity3d.com/6000.6/Documentation/Manual/Prefabs.html): 재사용, 중첩, 변형, 인스턴스별 변경 | 캐릭터·상자·대화창 묶음을 재사용하고 원본과 개별 변경을 구분 |
| [ScriptableObject](https://docs.unity3d.com/6000.6/Documentation/Manual/class-ScriptableObject.html): GameObject와 독립된 공유 데이터 에셋과 Inspector 참조 | 캐릭터 기본 정의와 소유 캐릭터·현재 체력을 분리하고 참조 선택 UI 제공 |

이 표는 Unity API를 우리 툴에 그대로 채택한다는 뜻이 아니다. 작업 흐름과 데이터 경계를 참고한다. UI 배치·애니메이션·타임라인·오디오·2D 물리는 추가 비교 후보이며 이번에 모두 직접 검증하지 않았다.

## 추가 검증과 기반 선택

Unity 사용성을 직접 비교하려면 연구용 프로젝트에서 선택 속성 편집, 장면 재사용, 재생/중지와 저장 상태 구분, 작은 Android 내보내기 흐름을 실측한다. 기존 설치 상태를 근거로 새 도구가 이미 실행 가능하다고 기록하지 않는다.

현재 추천은 Godot을 우선 시제품 후보로 놓고 이 설치된 Unity와 비교하는 것이다. 최종 엔진·에디터 실행 형태는 미정이며, [기초 설계](../../TOOL_FOUNDATION.md)의 RPG Maker 중심 기능·UI 방향과 실행 엔진 선택은 서로 다른 결정이다.
