# 참고 소스 (분석·테스트용)

2026-10-26 추가: `godot-4.7.2/`에 공식 portable Windows 배포본과 동일 커밋의 핵심 소스 일부·LICENSE·`acquisition.json` 보관. 커밋 `ed1daf0bf001b61586d9930840f2f1394092c079`, MIT. 전체 저장소가 아니며 manifest의 404 파일은 미수집이다. 실행 파일/ZIP은 해당 폴더의 `.gitignore`로 Git 제외하고 로컬에 보존한다. [조사 기록](../analysis/godot/README.md) 참고.

게임 툴 설계 분석에 사용한 외부 오픈소스 원본이다. 나중에 다시 분석하거나 동작을 테스트할 때 사용한다. **우리 툴 코드에 복사하지 않는다** — 구조·개념만 참고한다.

| 폴더 | 원본 | 커밋 | 라이선스 | 용도 |
|---|---|---|---|---|
| `rpgmaker-mv-corescript/` | https://github.com/rpgtkoolmv/corescript | `182e31449707ba7e406db0485c44c2a9d11e2dcd` | MIT (KADOKAWA / YOJI OJIMA) | RPG Maker MV 런타임 전체 JS 소스 |
| `mkxp/` | https://github.com/Ancurio/mkxp | `6a875b30d20728cf67f3690bef4c006790f4d682` | GPL-2.0 | RPG Maker XP/VX/VX Ace RGSS 런타임 오픈소스 재구현 (내장 API·RPG 데이터 클래스) |

- 받은 날짜: 2026-10-26 (`git clone --depth 1`). 원본 저장소의 `.git` 폴더는 제거하고 파일만 보관했다.
- 각 폴더의 `LICENSE` / `COPYING` 파일을 지우지 않는다.
- 이 저장소는 프라이빗이며 외부 배포하지 않는다. GPL 코드(mkxp)를 우리 툴에 섞으면 우리 툴도 GPL 조건을 따라야 하므로 섞지 않는다.
- XP / VX / VX Ace / MZ의 기본 스크립트·코어 스크립트·에디터 바이너리는 공개 라이선스가 아니므로 보관하지 않는다.
- 분석 결과 문서: `../analysis/rpgmaker/`
