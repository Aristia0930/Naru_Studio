# Unity REA 관측과 공개 참고 소스 조사

2026-10-27 사용자 추가 요청으로 수행했다. 설치 확인/공식 문서만 조사했던 01 README의 범위를 보강한다. 전체 엔진 분석이나 C# 원본 복원 완료를 뜻하지 않는다.

## 1. REA에서 실제로 호출한 것

`reverse-engineer-anything` 지침에 따라 설치 아티팩트의 정적 관측과 공개 소스 읽기를 구분했다. REA 재설치 없이 현재 MCP 도구를 직접 호출했다.

- 대상: `C:\Program Files\Unity\Hub\Editor\6000.6.5f1\Editor\Data\Managed\UnityEditor.dll`
- 파일 크기: 23,752,192바이트.
- SHA-256: `5f5648cf51994a87c48b512522556f1489b94c80c1a87337ebde8a92b185ef2b`.
- MVID: `797618a9-0c5e-4590-b3ab-208db85a8631`.
- `inspect_managed_artifact(detail: summary)`: 관리형 CIL/Unity-Mono 분류, managed architecture AnyCPU, `.NETStandard,Version=v2.1`, 기본 메타데이터 coverage complete/issue 0.
- PE machine x86 표기와 managed AnyCPU를 구분한다. 이것만으로 설치된 에디터 프로세스가 x86이라고 해석하지 않는다.
- `inspect_managed_members`에 아래 7개 정확한 타입을 지정했다. unmatched 타입 0개. `inspect_analysis_view`의 types 페이지를 읽었다.
- 결과의 권고에 따라 SerializedObject 1개로 좁혀 추가 호출했지만 같은 전역 표현 한도가 발생했다. 동일 호출을 반복하거나 분석 한도를 임의 변경하지 않았다.
- 이 호출들은 어셈블리를 로드/실행하거나 C#을 디컴파일하지 않았다.

| 설치본에서 관측한 TypeDef | 선언의 method-list 행 수 | 주의 |
|---|---:|---|
| UnityEditor.EditorApplication | 162 | 행 범위이며 본문 분석 개수가 아님 |
| UnityEditor.InspectorWindow | 57 | 동일 |
| UnityEditor.Editor | 117 | 동일 |
| UnityEditor.PrefabUtility | 365 | 동일 |
| UnityEditor.SerializedObject | 84 | 동일 |
| UnityEditor.SerializedProperty | 463 | 동일 |
| UnityEditor.Undo | 92 | 동일 |

## 2. REA 분석 한계

members 관측은 metadata representation **67,108,864바이트 한도**로 partial이다. 타입 목록 이후 fields/methods/member_refs/call_edges 등의 디코딩을 끝내지 못했다. 이 결과의 빈 배열이나 methods 0을 실제 메서드 부재로 해석하지 않는다.

타입 이름·선언 위치/행 범위는 관측했지만 **메서드 시그니처/본문/호출 그래프를 REA로 검증했다고 주장할 수 없다**. 기본 artifact 메타데이터 검사의 complete와 members 검사의 partial은 서로 다른 범위다. 네이티브 Unity 코어 동작과 런타임 실행은 미관측이다.

## 3. 근거 파일

- [artifact 요약](rea-artifact-summary.json): 부모 `ev_de68c20982a6dd83f772b7596b14290898136e0da83d4c63be093c5ef490d05e`.
- [7타입 members 요약](rea-members-summary.json): 부모 `ev_959e4fd4f624d219b9168b087fe68c97238f9a651845a23624d8dfc14cd5f1f2`.
- [관측한 타입 페이지](rea-selected-types.json): `ev_85b9ca1c227e6c44b1deba9773ec85413b93e814d7a7e311a020bac5d4f6bfd3`.
- [1타입 축소 확인](rea-single-type-summary.json): 부모 `ev_ac0560b878b516bab2bfb233e6f503c143e07cc426857af632dcad093bb0a913`.
- [전체 REA 번들](rea-evidence-20261027.json): 이 작업의 artifact/members/view 7건. 15,708,771바이트. 번들 최상위 unknowns 0은 members가 완전하다는 의미가 아니다.

## 4. 공식 공개 참고 소스와 대조

저장소: [UnityCsReference](https://github.com/Unity-Technologies/UnityCsReference).

고정 커밋: `bf6028387a4fc2ea6d41aa7d8998e774655aedf6`. README의 버전은 **Unity 7000.0.0a12**이며 설치본 6000.6.5f1과 동일 빌드로 검증된 것이 아니다. 아래는 그 공개 스냅샷의 소스 관측이다. REA가 이 소스를 자동 분석했다거나 설치 DLL의 동일 구현으로 매핑됐다고 설명하지 않는다.

[수집 manifest](../../refs/unity-csreference/acquisition.json)에 커밋·각 URL·SHA-256·크기를 보관했다. 공개 소스 원문은 로컬 참고용이며 `refs/unity-csreference/source/`를 Git 제외한다. 다른 PC에서는 [수집 스크립트](../../tools/acquire_unity_reference.py)로 다시 받는다. 원문을 제품 코드로 복사하지 않는다.

| 공개 소스 경로 (커밋 기준) | 직접 읽은 관측 | Naru C++/Qt에 반영할 설계 제안 |
|---|---|---|
| `Editor/Mono/Inspector/Editor.cs:984` | 기본 인스펙터가 객체 갱신 후 보이는 속성을 순회하고 필드를 그린 뒤 변경을 적용 | 데이터 스키마→속성 어댑터→Qt 필드 생성→명령으로 반영하는 공통 인스펙터 |
| `Editor/Mono/SerializedObject.bindings.cs:116,130,175,192` | 속성 iterator/path 조회와 update/apply API, 일부는 native extern 선언 | 객체 ID + 속성 경로로 읽기/쓰기 경계 정의. extern의 네이티브 내부 동작은 추정하지 않기 |
| `Editor/Mono/Undo/Undo.bindings.cs:93,184,215,242` | 완전 객체/변경 기록 API, 그룹 시작/합치기 선언 | 드래그 여러 프레임을 한 Undo 명령으로 묶고 명령 이름/이전·이후 값을 보존 |
| `Editor/Mono/Prefabs/PrefabUtility.cs:311` | instance 검증 후 atomic scope에서 원본과 인스턴스의 계층 Undo 등록 및 적용 경로 | 원본 묶음·인스턴스·개별 변경값 분리, 원본 적용/개별 되돌리기를 하나의 편집 트랜잭션으로 |
| `Editor/Mono/Inspector/Core/InspectorWindow.cs:134` | 프로젝트 로드·재생 상태·선택 변경·어셈블리 재로드 이벤트 구독 | 선택/프로젝트/실행 상태가 바뀔 때 인스펙터를 갱신하고 창 종료 시 구독 해제 |
| `Editor/Mono/EditorApplication.bindings.cs:81` | 플레이 상태와 진입/종료 API가 분리되어 있음 | 자체 엔진 Session 시작·중지·실패 상태와 Qt 편집 상태를 명시적으로 연결 |

## 5. 이번 결론과 다음 구현 연결

Unity에서 참고할 핵심은 화면 모양보다 **선택 대상의 속성 모델, 변경 적용 경계, 묶음 Undo, 재사용 원본과 인스턴스의 변경 추적, 플레이 상태 전환**이다. 이들은 Naru 자체 엔진의 데이터/명령 모델과 Qt UI 사이에 분리된 인터페이스로 구현할 후보다.

이 조사로 C++/Qt/Python 확정을 바꾸지 않는다. 다음 구축은 [NARU_NEXT_STEPS.md](../../NARU_NEXT_STEPS.md)의 최소 CMake 엔진·Qt 창·Python 임베딩부터다. 인스펙터 구현 전에 위 속성/명령 경계를 실제 C++ 타입과 테스트로 구체화한다.

남은 Unity 검증: 정확히 일치하는 공개 참고 리비전 확보 여부, members 상세 용량 제한에 맞는 별도 API/XML 관측, 실제 UI 작업 흐름 비교. 필수 근거가 없는 영역을 완전 분석으로 표시하지 않는다.
