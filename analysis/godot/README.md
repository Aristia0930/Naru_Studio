# Godot 4.7.2 조사 — 실행 검증과 제작툴 설계 근거

2026-10-26. 핵심 질문은 씬·리소스·이벤트 연결·2D 이동이 공통 제작툴 기반으로 쓸 수 있는지다. 전체 기능 조사 완료를 뜻하지 않는다.

## 대상 식별과 다운로드

- [공식 Windows 다운로드](https://godotengine.org/download/windows/)에서 확인한 4.7.2 standard x86_64를 사용했다. .NET SDK가 필요한 C# 배포본은 선택하지 않았다.
- [공식 릴리스](https://github.com/godotengine/godot-builds/releases/tag/4.7.2-stable)의 ZIP 86,013,866바이트를 `refs/godot-4.7.2/`에 다운로드했다.
- ZIP SHA-256 `731980f9608d61333e5baf54a2ef17210acc7a538446c0cb9969f002aca1e953`, GitHub asset digest와 일치.
- 본체 EXE SHA-256 `ab1824f85bfd8e0e4128182c000c4003a3e042245b2967848d089b2a04b22424`.
- 콘솔 래퍼 SHA-256 `c8f0a6bc45a19b33541501e57f6f7cd972ab18453743266339d495cbbe846643`.
- 실행 출력과 소스 태그의 커밋은 `ed1daf0bf001b61586d9930840f2f1394092c079`로 일치했다.
- [수집 manifest](../../refs/godot-4.7.2/acquisition.json)에 URL·해시·파일 목록을 보관했다. 핵심 C++ 10개와 LICENSE를 수집했으며 전체 소스가 아니다. 요청한 `editor/scene/scene_tree_dock.cpp`는 404였고 오류도 manifest에 남겼다. 해당 파일로 확인한 주장은 없다.
- 재현: `python engine-tool/tools/acquire_godot_research.py`. 네트워크 접근 필요. 같은 태그의 ZIP이 있으면 digest를 검증해 재사용한다.

## 소스로 확인한 구조와 우리 설계 해석

아래 경로는 `refs/godot-4.7.2/source/` 기준이다. [관측 JSON](../foundation/source-observations.json)에 파일 해시·행·제한된 발췌를 저장했다.

| 직접 읽은 위치 | 관측 | 우리 설계에 주는 의미 — 분석자 제안 |
|---|---|---|
| `scene/main/node.cpp:1711` | add_child가 부모/자식 구조를 구성 | 장면 트리로 배치 대상을 선택하고 속성을 편집 |
| `scene/resources/packed_scene.cpp:155`, `2473`, `2507` | SceneState 기반 pack/instantiate, 재구성된 노드 반환 | 장면 재사용, 원본과 실행 인스턴스 분리 |
| `core/object/object.cpp:1179`, `1518` | 시그널 발생·연결, callable·대상·시그널 검증 | UI 이벤트 연결의 타입/참조 검사와 해제 필요 |
| `scene/main/scene_tree.cpp:639`, `688` | physics/process가 다른 갱신 경로, 각각의 timer/tween 처리 | 물리·표시 시간 구분, 일시정지와 대기 정책 명시 |
| `scene/2d/physics/character_body_2d.cpp:43` | move_and_slide가 프레임 delta와 충돌 결과 사용 | 횡스크롤 이동·충돌은 엔진 기반으로 재사용 가능 |
| `editor/inspector/editor_inspector.cpp:5256` | 선택 객체를 바꾸고 property_list_changed를 연결·해제 | 선택 기반 속성 폼과 데이터 변경 통지 |
| `editor/editor_undo_redo_manager.cpp:305`, `332` | history별 undo/redo 관리와 version_changed 통지 | 모든 편집을 복구 가능한 명령으로 처리 |

[노드·씬 문서](https://docs.godotengine.org/en/stable/getting_started/step_by_step/nodes_and_scenes.html)와 [시그널 문서](https://docs.godotengine.org/en/stable/getting_started/step_by_step/signals.html)도 비교했다. 문서의 stable 별칭은 바뀔 수 있으므로 정확한 버전 근거는 로컬 커밋·실행 결과다.

## 실제 실행 검증

코드: [probe.gd](../../probes/godot-foundation/probe.gd), 실행: `python engine-tool/tools/run_godot_probe.py`.

결과: [probe-result.json](../../probes/godot-foundation/probe-result.json), [프로세스 기록](runtime-probe-process.json). 종료 코드 0, 최종 stderr 비어 있음.

| 검증 | 결과 |
|---|---|
| 씬 pack, `.tscn` 저장 | 2개 통과 |
| 저장 시 owner가 지정된 자식 포함, 미지정 자식 제외 | 통과 |
| 같은 PackedScene의 인스턴스 2개 위치 독립 | 통과 |
| 시그널 전달, disconnect 후 전달 중단 | 2개 통과 |
| `.tres` 저장, 이름/메타데이터 재로드 | 2개 통과 |
| 90 physics ticks 수평 이동과 벽 충돌 | 통과, 최종 x 약 69.9975 |
| Camera2D가 캐릭터 자식이고 enabled | 통과 |

총 10개. 처음 샌드박스 실행은 사용자 데이터 폴더·루트 인증서 접근 오류를 출력했다. 승인된 사용자 환경에서 같은 코드를 재실행해 stderr 없는 결과로 갱신했다. 사용자 설정·설치 경로·전체 Java 환경은 변경하지 않았다.

시각적 UI·렌더링·카메라 화면 추적·음원 청취·애니메이션·점프·턴제 전투·Android 내보내기는 검증하지 않았다. 이 작은 실행 검증을 게임툴 구현 완료로 해석하지 않는다.

## REA와 Ghidra로 확인한 출시 바이너리

1. REA `inspect_native_exports`로 본체의 PE export 3개를 확인했다. `AmdPowerXpressRequestHighPerformance`, `NvOptimusEnablement`는 데이터 export이며 `NoHotPatch`도 장면/에디터 구조 설명의 근거가 아니다. [원시 결과](rea-exports.json), Evidence `ev_df46345c41283e3b883aebd21ccb79c8138f2176501171b44fcbb85ef9674a38`.
2. `open_binary(provider_id: ghidra)`로 **콘솔 래퍼**를 열었다. PE 진입점 `0x140001400`에서 `0x140001010` 초기화를 거쳐 `0x1400086a0` 실행 처리 함수로 이어지는 의사 코드를 얻었다.
3. 마지막 함수에 본체 경로 구성, 인자 획득, `CreateProcessW`, Job Object 연결, 완료 통지 대기, `GetExitCodeProcess` 반환이 나타났다. 이는 래퍼가 본체 실행·로그·종료 결과 수집에 사용된다는 정적 근거다. 원래 소스의 완전 복원이나 모든 에러 경로의 런타임 검증은 아니다.
4. 근거를 내보내고 `close_binary`로 세션을 닫았다.

Evidence: 진입점 `ev_ae676b5169bef96f93eacbb33fa9acacc63ec4581df01ab9fc2c4bc630b9e5ec`, 초기화 `ev_22bbf3337cf19d7cb1e72ccde2312f5b2526425c33971d52e5df333e45abb192`, 실행 처리 `ev_bd84e7473e581448c1111e81d72b4234eb9d9ef46c5f720f976f0c1aea566164`.

[전체 번들](ghidra-launcher-evidence-20261026.json)은 같은 세션에 남아 있던 MV 2건·본체 exports 1건·래퍼 의사 코드 3건의 총 6건이다. 전체가 Godot Ghidra 결과인 것은 아니다. [개별 실행 함수](ghidra-launcher-worker.json)도 별도로 저장했다.

소스로 답할 수 있는 씬·에디터 구조는 소스를 읽었고, Ghidra 결과로 그 구조까지 검증했다고 과장하지 않는다. 우리 툴의 '실행' 버튼은 프로젝트 경로·인자를 명시하고 로그·종료 코드·중지 처리를 관리해야 한다는 설계 제안으로 연결한다.

## 배포 확인 범위

음악·애니메이션은 [AnimationPlayer와 키프레임](https://docs.godotengine.org/en/stable/tutorials/animation/introduction.html), [Audio buses](https://docs.godotengine.org/en/stable/tutorials/audio/audio_buses.html) 공식 문서로 비교했다. 속성 트랙과 오디오 버스를 우리 UI로 제공할 후보 근거다. 실제 애니메이션 재생·음원 청취 검증은 다음 시제품에서 수행한다.

[Windows export](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html), [Android export](https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_android.html)를 확인했다. 엔진 export가 있어도 export templates·대상 SDK·서명·에셋 수집·실기기 점검이 필요하다. 이번에는 템플릿/SDK 추가 다운로드나 APK 빌드를 하지 않았다.

다음 실험은 RPG Maker식 데이터/이벤트 UI를 Godot 확장으로 만들지 독립 앱으로 만들지 비교하는 작은 편집→저장→미리보기 흐름이다. [기초 설계](../../TOOL_FOUNDATION.md)를 따른다.
