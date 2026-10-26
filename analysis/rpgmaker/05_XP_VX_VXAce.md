# RPG Maker XP / VX / VX Ace 구조·장점 분석 (전용 제작 툴 설계용)

- 분석일: 2026-10-26
- 목적: 2D 카드 수집형 턴제 RPG + 비주얼노벨식 스토리(대사·CG·BGM) 게임용 **전용 제작 툴**의 뼈대 설계 참고. 타일맵·필드 이동은 범위 밖이라 간단히만 언급함.
- 범위: XP(RGSS1), VX(RGSS2), VX Ace(RGSS3). MV(JS)는 다른 문서에서 소스 분석 중이므로 비교 대상으로만 언급함.
- 자료 원칙: 상용 에디터 바이너리와 RGSS 기본 스크립트(Game_*/Scene_* 등) 사본은 받거나 열람하지 않았음. 기본 스크립트 계층 설명은 공개 문서, 커뮤니티 설명, 공개 스크립트 소개 페이지에 나온 클래스명 수준에 그침. 데이터 구조는 오픈소스 mkxp(GPL)에 포함된 `RPG` 모듈 재구현(`binding-mri/module_rpg{1,2,3}.rb.xxd`)과 내장 클래스 바인딩으로 확인함.
- 표기: **[mkxp]**는 로컬 mkxp 소스로 직접 확인한 내용, **[웹]**은 아래 출처로 확인한 내용, **미확인**은 공개 자료로 검증하지 못한 내용.

## 출처

공식 및 준공식
- https://www.rpgmakerweb.com/products/rpg-maker-xp (공식 제품 페이지. WebFetch 파싱 오류로 검색 요약만 확인)
- https://www.rpgmakerweb.com/products/rpg-maker-vx (위와 동일)
- https://rpgmakerweb.com/products/rpg-maker-vx-ace (위와 동일. "flexible Features system", 캐릭터 생성기, 복수 타일셋)
- https://old.rpgmakerweb.com/products/programs/rpg-maker-xp (검색 요약)
- https://rpgmakerofficial.com/product/MV_Help/page/01_08_15.html (MV 공식 도움말 "Traits 설정". VX Ace Features의 후계 구조 비교용)
- https://forest.watch.impress.co.jp/docs/news/20111205_496009.html (窓の杜, VX Ace 발표 기사 2011-12-05. 신기능 목록)

위키·커뮤니티
- https://en.wikipedia.org/wiki/RPG_Maker_XP (시리즈 연표, 해상도, fps)
- https://en.wikipedia.org/wiki/RPG_Maker_VX (VX/VX Ace 출시일과 변경점)
- https://wikiwiki.jp/viprpg-dev/XP , https://wikiwiki.jp/viprpg-dev/VX , https://wikiwiki.jp/viprpg-dev/VXAce (일본 커뮤니티 위키. 장단점, 내장 Ruby 버전)
- https://rpgmaker.fandom.com/wiki/RPG_Maker_VX_Ace (검색 요약만 확인. 본문은 402로 접근 불가)
- https://rpgmaker.net/forums/topics/14898 (XP vs VX vs VX Ace 토론. 검색 요약만 확인)
- https://rpgmaker.net/forums/topics/17557 (XP 전투 클래스 구성 언급)
- https://rpgmaker.net/scripts/77 (Yanfly Ace Battle Engine)
- https://rpgmaker.net/scripts/78 (Yanfly Enemy HP Bars)
- https://rpgmaker.net/articles/1237 (Frontview or Sideview?)
- https://steamcommunity.com/app/220700/discussions/0/864961722069290024 (VX Ace 사이드뷰 스크립트 추천 스레드)
- https://steamcommunity.com/app/220700/discussions/0/1629664606984104275 (VX Ace 해상도 640x480 상한 논의)
- https://galvs-scripts.com/galvs-animated-battlers/ (VX Ace 애니메이션 배틀러 스크립트)
- https://anya-and-lolo.itch.io/lost-vx-ace-scripts (VX Ace 스크립트 아카이브. ATB/CTB 등 분류)
- https://raizen884.itch.io/triple-triad-for-rpg-maker-mz (카드 게임 플러그인 사례. MZ용, 비교 참고)
- https://www.rubydoc.info/gems/rgss3/index (서드파티 RGSS3 gem 문서)
- https://dic.pixiv.net/a/RGSS (검색 요약만 확인. 본문은 403)

오픈소스
- mkxp (GPLv2+), 로컬 `C:\Users\DKSYSTEMS\Desktop\개인\engine-tool-refs\mkxp` — `README.md`, `binding-mri/*.cpp`, `binding-mri/module_rpg{1,2,3}.rb.xxd`, `src/*.h`

---

## 1. 버전별 개요

| 항목 | RPG Maker XP | RPG Maker VX | RPG Maker VX Ace |
|---|---|---|---|
| 출시 (일본 / 영문) | 2004-07-22 / 2005-09-16 [웹: Wikipedia] | 2007-12-27 / 2008-02-29 [웹] | 2011-12-15 / 2012-03-15 [웹] |
| 스크립트 언어 | RGSS1 | RGSS2 | RGSS3 |
| 내장 Ruby | 1.8.1 [웹: wikiwiki] | 1.8.1 [웹: wikiwiki]. pixiv 백과는 "RGSS2 이하 1.8.3"이라 하여 출처 간 불일치, 정확한 값 **미확인** | 1.9.2 [웹: wikiwiki, 검색 요약] |
| 게임 해상도 | 640x480 고정 (Wikipedia). mkxp에서도 `Graphics.width/height/resize_screen`이 RGSS2 이상에서만 정의됨 [mkxp `graphics-binding.cpp`] | 544x416 기본, `Graphics.resize_screen` 도입 [mkxp]. VX의 상한값은 **미확인** | 544x416 기본, `resize_screen`으로 최대 640x480 (커뮤니티 다수 증언, 공식 문서로는 **미확인**) |
| 기본 프레임레이트 | 40fps (Wikipedia) | 60fps (Wikipedia) | 60fps (VX 계승으로 추정, **미확인**) |
| 기본 전투 | 프론트뷰 턴제. 적과 아군 모두 "배틀러" 일러스트로 표시. 아군 배틀러는 하단 상태창 영역 위에 표시 [웹: 제품 페이지/프리뷰 요약] | 프론트뷰 턴제(드래곤퀘스트형). 아군 그래픽 미표시, 행동을 텍스트로 상세 서술. 전투 배경 없음 (Wikipedia, 커뮤니티) | 프론트뷰 턴제(VX 계열). 전투 배경 부활(근경/원경 2장), TP 도입, 5인 이상 파티 가능하나 전투 참가는 4명 [웹: 窓の杜] |
| 데이터 파일 확장자 | `.rxdata` | `.rvdata` | `.rvdata2` (mkxp에서 스크립트 섹션명 포맷만 확인. 확장자는 커뮤니티 통념, 공식 문서로는 **미확인**) |
| 대표 커뮤니티 평가 | 고급 사용자용, 2000/2003의 이벤트 명령 일부 삭제, 스크립트 소재 배포 문화의 시작 [wikiwiki] | 이벤트 명령 상당수 복원으로 입문자 친화. 단 복수 타일셋 불가 등 제약 [wikiwiki, Wikipedia] | "안정적이고 정리가 잘 됨, 스크립트 소재 풍부", MV 이후에도 사용자 잔존 [wikiwiki] |

참고: MV(2015)는 JS + 플러그인으로 바뀌고 기본 전투가 사이드뷰가 됨. 데이터 구조(Traits/Effects/Damage formula/Note)는 VX Ace를 거의 그대로 JSON으로 옮긴 형태임 (MV 공식 도움말 Traits 항목 구성과 일치).

---

## 2. 런타임 아키텍처

### 2.1 두 계층 구조

```
[에디터]  데이터베이스/맵/이벤트 편집  ──저장──>  Data/*.rxdata|rvdata|rvdata2 (Ruby Marshal)
                                                  Data/Scripts.* (스크립트 섹션 배열)
[런타임 Game.exe + RGSSxxx.dll]
  ├─ 내장 계층 (C++/DLL, 수정 불가)   : Bitmap, Sprite, Window, Viewport, Plane, Tilemap,
  │                                     Graphics, Input, Audio, Table, Color, Tone, Rect, Font,
  │                                     RPG 데이터 클래스(RPG::Actor 등), Marshal 로딩 함수
  └─ 스크립트 계층 (Ruby, 에디터에서 전부 수정 가능)
        Game_*   : 게임 상태/로직 (Game_Battler, Game_Party, Game_Troop, Game_Interpreter …)
        Sprite_* / Spriteset_* : 상태를 화면 오브젝트로 그리는 뷰
        Window_* : UI 창 (메뉴, 메시지, 전투 커맨드 …)
        Scene_*  : 화면 단위 상태 머신 (Title, Map, Menu, Battle …)
        Main     : 진입점
```

- 기본 스크립트 계층의 클래스 이름(예: XP의 `Game_BattleAction`, `Game_Battler`, `Game_Enemy`, `Game_Troop`, `Scene_Battle`, `Sprite_Battler`, `Spriteset_Battle`, `Window_BattleResult`, `Window_BattleStatus`)은 커뮤니티 문서에서 확인함 (rpgmaker.net/forums/topics/17557). 메서드 구현은 라이선스 문제로 열람하지 않았음.
- VX Ace에서 전투 흐름이 `BattleManager` 모듈로 분리된 것은 커뮤니티 해설(rpgmaker.net "Slip into Ruby" 시리즈 언급)로만 확인. 세부는 **미확인**.
- 공개 스크립트를 보면 사용자 확장 방식은 기본 클래스를 다시 열어 메서드를 덮어쓰거나 alias로 감싸는 것임. 예: Galv's Animated Battlers는 `class Game_Interpreter`를 다시 열어 `change_battler(id,name)`를 추가함.

### 2.2 스크립트 로딩 방식 [mkxp `binding-mri/binding-mri.cpp` `runRMXPScripts`]

- `Game.ini`가 가리키는 스크립트 팩(`Data/Scripts.rxdata` 등)을 Marshal로 읽음. 내용은 `[id, 이름, zlib 압축 소스]` 배열이며, 전역 `$RGSS_SCRIPTS`에 저장됨.
- 각 섹션을 zlib로 풀어 **위에서 아래 순서대로 `eval`** 함. 섹션 이름은 에러 표시용으로만 쓰이며 RGSS1/2는 `Section%03d`, RGSS3은 `{%04d}` 포맷임.
- 즉 "모듈 시스템"이 없음. **순서가 곧 의존성이고 나중 섹션이 앞 섹션을 덮어씀.** 커뮤니티의 "Materials(소재) 섹션 아래, Main 위에 넣으세요" 관례가 여기서 나옴.
- RGSS3에는 `rgss_main { }` / `rgss_stop` / `msgbox` / `msgbox_p`가 Kernel에 추가됨 (F12 리셋 처리용 루프 래퍼).

### 2.3 "기본 시스템 자체를 스크립트 에디터로 수정" 방식의 장단점

| 장점 | 단점 |
|---|---|
| 전투·메뉴·메시지 등 **모든 게임 규칙이 사용자 코드**라 사이드뷰/ATB/카드 배틀 같은 완전히 다른 시스템도 만들 수 있음 (XP 제품 설명: "RGSS 스크립트를 편집해 모든 기능을 변경·추가 가능") | 엔진 업데이트와 사용자 수정이 분리되지 않음. 기본 스크립트를 직접 고치면 공식 버그 수정을 받기 어려움 |
| 별도 SDK나 빌드 없이 에디터 안에서 곧바로 고치고 테스트 플레이 가능 | 모듈·네임스페이스·의존성 선언이 없어서 스크립트 간 **충돌**이 잦음 (같은 메서드 덮어쓰기). Yanfly Core처럼 "코어 스크립트 필수" 계열이 생기고 순서 지정이 사실상 패키지 관리자 역할을 함 |
| 커뮤니티 스크립트 생태계 (XP부터 스크립트 소재 배포 시작, Ace에서 가장 풍부) | RGSS1/2/3 간 **호환 불가**. 버전마다 기본 스크립트 구조가 크게 바뀌어 XP 스크립트가 VX에서 그대로 동작하지 않음 (커뮤니티 증언) |
| 데이터(에디터)와 로직(스크립트)이 분리돼 있어 기획자는 DB를, 프로그래머는 스크립트를 다룰 수 있음 | 에디터는 스크립트가 추가한 개념을 모름. 새 기능의 설정값은 Note 태그나 스크립트 상단 설정 모듈로 넣어야 해서 **에디터 UI가 확장되지 않음** (아래 3장) |
| 텍스트 기반이라 비교적 이식성이 높음 (mkxp가 기본 스크립트만 쓰는 게임은 그대로 실행 가능하다고 명시) | 에디터 내장 스크립트 편집기는 단일 텍스트 영역 수준이고 VCS·diff가 어려움 (Scripts 파일이 Marshal+zlib 바이너리). 정확한 편집기 기능은 **미확인** |

우리 툴에 주는 시사점: "모든 규칙을 사용자 코드로"는 유지하되 **엔진 코어와 게임 규칙 모듈의 경계, 명시적 확장 지점(훅/이벤트), 텍스트 파일 기반 저장**을 처음부터 넣어야 함. MV는 플러그인 파일 분리와 플러그인 매니저 파라미터 UI로 일부 해결함.

### 2.4 mkxp에서 확인한 RGSS 내장 API 구조

mkxp `binding-mri.cpp`의 `mriBindingInit()`은 Table, etc(Color/Tone/Rect), Font, Bitmap, Sprite, Viewport, Plane, Window(RGSS1) 또는 WindowVX(RGSS2 이상), Tilemap 또는 TilemapVX, Input, Audio, Graphics, 파일 I/O를 등록함. 버전 분기는 `rgssVer` 하나로 처리함. 각 바인딩에서 등록한 메서드는 다음과 같음.

| 클래스/모듈 | 주요 API (mkxp 바인딩 기준) | 버전 차이 | 우리 툴 관점 |
|---|---|---|---|
| `Bitmap` | `blt`, `stretch_blt`, `fill_rect`, `gradient_fill_rect`, `clear_rect`, `get/set_pixel`, `hue_change`, `blur`, `radial_blur`, `draw_text`, `text_size`, `font` | `gradient_fill_rect`, `clear_rect`, `blur`, `radial_blur`는 RGSS2 이상 (`bitmap-binding.cpp`의 `rgssVer >= 2` 분기) | CPU 측 이미지 버퍼 + 텍스트 렌더. 카드 일러스트 합성, 동적 카드 프레임 생성에 해당 |
| `Sprite` | `bitmap`, `src_rect`, `x/y`, `ox/oy`, `zoom_x/y`, `angle`, `mirror`, `opacity`, `blend_type`, `color`, `tone`, `bush_depth/opacity`, `wave_*`, `flash`, `update` | `width/height`, `bush_opacity`, `wave_*`는 RGSS2 이상 (`sprite-binding.cpp`) | 화면 오브젝트 1개 = 변환 + 색 효과. 카드 뒤집기·확대·플래시 연출의 최소 단위 |
| `Viewport` | `rect`, `ox/oy`, `color`, `tone`, z순서 | 공통 | 레이어(배경/캐릭터/UI/대사창) 클리핑과 색조 일괄 적용. VN 화면 레이어 구조에 그대로 쓸 만함 |
| `Plane` | 무한 반복 타일 이미지, `zoom`, `opacity`, `tone` | 공통 | 스크롤 배경 |
| `Window` (RGSS1) / `WindowVX` (RGSS2+) | `windowskin`, `contents`(Bitmap), `cursor_rect`, `active`, `pause`, `opacity/back_opacity/contents_opacity`; VX 계열은 `openness`, `open?/close?`, `padding`, `arrows_visible`, `tone`, `move` 추가 | RGSS2에서 열림/닫힘 애니메이션(`openness`)과 패딩이 내장으로 들어감 | **윈도우스킨 한 장으로 UI 일관성을 확보**하는 방식은 1인 개발에 효율적. 대사창·선택지·카드 정보창에 응용 |
| `Graphics` | `update`, `freeze`, `transition`, `frame_rate`, `frame_count`, `brightness`, `frame_reset`; RGSS2+ `width/height`, `wait`, `fadeout/fadein`, `snap_to_bitmap`, `resize_screen`; RGSS3 `play_movie` | 해상도 가변은 RGSS2부터, 동영상은 RGSS3부터 | `freeze` + `transition(시간, 마스크 이미지)` 조합은 VN 장면 전환에 그대로 쓸 수 있는 좋은 추상화 |
| `Audio` | `bgm/bgs/me/se`의 `_play/_stop/_fade`; RGSS3 `bgm_pos/bgs_pos` 및 위치 지정 재생 | 재생 위치 기억은 RGSS3부터 | **BGM/BGS/ME/SE 4채널 구분**은 VN+RPG에 적합. ME(짧은 징글) 재생 중 BGM 일시정지 같은 의미를 엔진이 앎 |
| `Input` | `update`, `press?`, `trigger?`, `repeat?`, `dir4`, `dir8`; RGSS3에서 심볼(`:C`, `:B` 등) 인자 | 공통 | 논리 버튼 추상화. 마우스는 원래 없음 (mkxp 확장으로만 `mouse_x/y`) |
| `Table` | `resize`, `xsize/ysize/zsize`, `[]`, `[]=` (최대 3차원 정수 배열, 직렬화 가능) | 공통 | XP의 `Actor#parameters`, `Class#element_ranks`, 맵 데이터 등에 쓰임. 우리 툴은 일반 JSON 배열로 대체 가능 |
| `Font`, `Color`, `Tone`, `Rect` | `name/size/bold/italic/color/shadow/outline/out_color` | `shadow`는 RGSS2 이상, `outline`/`out_color`는 RGSS3 (`font-binding.cpp`) | 텍스트 스타일 기본 단위 |

내장 계층이 "렌더링 원시 요소 + 오디오 + 입력 + 직렬화"만 제공하고 게임 개념(전투, 메시지, 메뉴)은 전부 스크립트 계층에 두는 경계는 **깨끗하게 잘 나뉜 설계**임. 우리 툴의 런타임도 이 경계를 따르는 것이 좋음.

또 `RPG` 모듈(데이터 클래스)은 내장 계층에 있음 [mkxp `module_rpg.cpp`가 버전별 Ruby 정의를 내장 로드]. 에디터와 런타임이 공유하는 **데이터 스키마가 하나의 명세로 고정**되어 있다는 뜻임. XP의 `RPG::Cache`, `RPG::Sprite`, `RPG::Weather`는 RGSS1에만 있고 RGSS2/3에서는 기본 스크립트 쪽(Cache 모듈 등)으로 옮겨짐 [mkxp, 이동 위치는 미확인].

---

## 3. 데이터베이스 변천

아래 필드 목록은 mkxp `module_rpg1/2/3`의 `initialize` 정의에서 추출함 [mkxp].

### 3.1 XP (RGSS1): 메커니즘마다 전용 필드, 속성·상태는 체크 집합과 등급 표

- **Skill**: `power`, `atk_f`, `eva_f`, `str_f`, `dex_f`, `agi_f`, `int_f`, `pdef_f`, `mdef_f`, `variance`, `hit`, `sp_cost`, `element_set`, `plus_state_set`, `minus_state_set`, `common_event_id`
  - 데미지 공식은 고정이고 사용자는 **계수(%)만** 조절함. 계산식 자체는 기본 스크립트에 있음 (공식 본문은 미열람, **미확인**).
- **Weapon**: `atk/pdef/mdef`, `str_plus…int_plus`, `element_set`, `plus_state_set`, `minus_state_set`
- **Armor**: `kind`, `auto_state_id`(장비 시 자동 상태 1개), `guard_element_set`, `guard_state_set`
- **Class / Enemy**: `element_ranks`, `state_ranks` (`Table`). 에디터에서는 속성·상태마다 A~F 등급으로 내성을 지정함. 등급별 배율 값은 기본 스크립트에 정의돼 있어 **미확인**.
- **State**: `restriction`, `nonresistance`, `zero_hp`, `cant_get_exp`, `cant_evade`, `slip_damage`, 각종 `*_rate`, `hold_turn`, `auto_release_prob`, `shock_release_prob`, `guard_element_set`, `plus/minus_state_set`
- **Note(메모) 필드 없음.** 커스텀 데이터는 스크립트 상단의 설정 해시(ID → 값)에 넣는 관례가 생김 (커뮤니티 관행, 구체 출처 **미확인**).
- 데이터베이스 탭에 Tileset·Animation·Common Event·System("Words" 용어)이 있음.

### 3.2 VX (RGSS2): 상속 계층 도입 + 불린 플래그 증식 + Note 등장

- 공통 상위 클래스 `BaseItem`(`id, name, icon_index, description, note`) → `UsableItem`(Skill/Item 공통: `scope, occasion, speed, base_damage, variance, atk_f, spi_f, physical_attack, damage_to_mp, absorb_damage, ignore_defense, element_set, plus/minus_state_set`) → `Skill`, `Item`. `Weapon`과 `Armor`도 `BaseItem`을 상속함.
- 아이콘이 파일명(`icon_name`)에서 **아이콘 시트 인덱스(`icon_index`)** 로 바뀜.
- 특수 능력이 **불린 필드로 하나씩 추가**됨: Weapon `two_handed, fast_attack, dual_attack, critical_bonus` / Armor `prevent_critical, half_mp_cost, double_exp_gain, auto_hp_recover` / Actor `two_swords_style, fix_equipment, auto_battle, super_guard, pharmacology, critical_bonus` / Enemy `levitate, has_critical`.
- **`note` 필드가 생김** (BaseItem, Enemy, State). 커뮤니티 스크립트가 `<태그>` 형식의 **Note 태그로 에디터 밖 설정을 주입**하는 문화가 여기서 시작됨.
- 능력치가 단순화됨 (XP `str/dex/agi/int` + `atk/pdef/mdef/eva` → VX `atk/def/spi/agi` + `hit/eva`). 용어 설정이 `Words`에서 `Terms`로 바뀜.
- 이벤트 페이지 조건에 `item_valid`, `actor_valid`가 추가되고, 트룹(전투 이벤트) 조건에 `turn_ending`이 추가됨.

### 3.3 VX Ace (RGSS3): Features / Effects / Damage(formula)로 일반화

- `BaseItem`에 **`features`(배열)** 와 `note`가 들어가고, `Actor`, `Class`, `Weapon`, `Armor`, `Enemy`, `State`가 **모두 `BaseItem`을 상속**함. 즉 "특성을 가질 수 있는 모든 것"이 같은 구조를 공유함.
- `RPG::BaseItem::Feature` = `(code, data_id, value)` **3-튜플**. 전용 필드와 불린 플래그를 하나의 범용 리스트로 대체함.
  - 공식 소개: 캐릭터·무기·방어구 DB에 "特徴(특징)" 항목을 추가해 능력치 변화 등 추가 효과를 설정 [窓の杜]. 공식 영문 페이지는 "flexible Features system"이라고 표현함.
  - 탭 구성(내성/능력치/공격/스킬/장비/기타)과 항목(속성 유효도, 약체 유효도, 상태 유효도, 상태 무효, 통상/추가/특수 능력치, 공격 속성, 공격 시 상태, 스킬 타입 추가/봉인, 장비 타입 장착/고정/봉인, 슬롯 타입, 행동 횟수 추가, 특수 플래그, 소멸 효과, 파티 능력 등)은 MV "Traits" 도움말과 같은 계열로 추정함. VX Ace 공식 문서로 항목명을 일일이 대조하지는 못했음 (**부분 미확인**).
  - code 번호와 의미의 대응표는 기본 스크립트(Game_BattlerBase 상수)에 있어 열람하지 않았음 (**미확인**).
- `RPG::UsableItem::Effect` = `(code, data_id, value1, value2)` 리스트. 窓の杜는 이를 "使用効果(사용 효과)" 항목으로 소개함. HP 회복, 상태 부여, 능력치 강화, 커먼 이벤트 호출 등을 **효과 목록**으로 쌓음.
- `RPG::UsableItem::Damage` = `type, element_id, formula(문자열), variance, critical`. mkxp 재구현의 `eval(a, b, v)`는 `Kernel.eval(@formula)`로 계산하고 결과를 `max(0)`로 자른 뒤 `rescue 0`으로 처리함 [mkxp]. 사용자는 `a.atk * 4 - b.def * 2` 같은 Ruby 식을 DB에 직접 씀 (계산식이 Ruby 문법을 따른다는 점은 [웹] 커뮤니티 위키 요약에서 확인).
- `RPG::System`에 **타입 목록**이 생김: `elements`, `skill_types`, `weapon_types`, `armor_types`(사용자 정의 문자열 배열). 스킬은 `stype_id`, 무기는 `wtype_id`, 장비는 `etype_id`로 타입을 참조함.
- 그 밖에 TP(`tp_cost`, `tp_gain`, `opt_display_tp`), `Actor#nickname`, `max_level`, `Class#params`(레벨별 능력치 곡선), `Enemy#drop_items`(배열), State의 해제 조건 세분화(`auto_removal_timing`, `min/max_turns`, `remove_by_damage`, `chance_by_damage`, `remove_by_walking`, `steps_to_remove`), `Map#note`, `Tileset#note`, 시스템 옵션 플래그(`opt_slip_death`, `opt_floor_death`, `opt_followers`, `opt_extra_exp` 등)가 추가됨.

### 3.4 무엇이 왜 바뀌었나 (해석)

| 문제 (이전 버전) | VX Ace의 해법 | 우리 툴에 주는 의미 |
|---|---|---|
| XP: 장비·상태가 할 수 있는 일이 필드로 고정됨 (방어구 자동 상태 1개, 무기 능력치 4종만 가산) | 어디서든 Feature를 여러 개 붙일 수 있음 | 카드·장비·패시브·버프를 **같은 Feature 리스트**로 표현하면 조합이 자유로움 |
| VX: 새 능력 = 새 불린 필드 → 스키마가 계속 커지고 액터/무기/방어구에 따로따로 존재 | 같은 code를 Actor·Class·Equip·State·Enemy 어디에 붙여도 의미가 같음. 런타임은 "모든 출처의 features 합산" | 효과 정의는 **하나의 레지스트리**, 출처는 여러 곳. 집계 규칙(합/곱/최대/OR)을 code별로 명시해야 함 |
| XP/VX: 데미지 공식이 스크립트에 고정되어 스킬마다 다른 공식을 쓰려면 스크립트 수정 필요 | 스킬별 `formula` 문자열 | 표현력은 크지만 `eval`은 디버깅과 검증이 어려움 (오류가 `rescue 0`으로 조용히 0이 됨). **검증되는 식 언어(타입체크, 미리보기)** 로 대체하는 것이 좋음 |
| 속성·무기 종류 등이 하드코딩된 종류 수 | System에 타입 목록 | 카드 타입, 속성, 레어도, 진영 등은 **사용자 정의 열거형**으로 둘 것 |
| 에디터가 모르는 확장 데이터 | `note` 필드를 모든 주요 엔티티로 확장 | Note 태그는 임시방편임. 우리는 **스키마 확장 필드(커스텀 필드 정의 → 에디터 폼 자동 생성)** 로 대체해야 함 |

MV의 Traits/Effects/Damage formula/Note는 이 구조를 거의 그대로 이어받음 (MV 공식 도움말 "How to Set Traits"의 Rate/Param/Attack 탭 구성과 대응). 즉 **VX Ace가 현재까지 이어지는 데이터 모델의 원형**임.

---

## 4. 이벤트 시스템과 에디터 UX

### 4.1 공통 구조 [mkxp]

- `RPG::EventCommand` = `(code, indent, parameters)`. 이벤트 스크립트는 **트리가 아니라 들여쓰기 깊이를 가진 평면 리스트**임. 분기와 반복의 끝은 별도 "종료" 명령 코드로 표현함. 이 구조는 XP, VX, VX Ace, MV 모두 같음.
- 이벤트를 쓰는 곳은 세 군데임: 맵 이벤트(페이지 + 조건 + 그래픽), 커먼 이벤트(`trigger`, `switch_id`), 트룹 이벤트(전투 중 페이지와 조건: 턴, 적 HP, 액터 HP, 스위치, `span`). 우리 게임에서 **전투 중 스토리 연출**은 트룹 이벤트에 해당함.
- 모든 버전에 "스크립트" 이벤트 명령과 조건 분기의 "스크립트" 조건이 있어 이벤트에서 Ruby를 직접 실행할 수 있음 (커뮤니티: Ace 조건 분기의 부족한 항목은 Script 옵션으로 보완).

### 4.2 버전별 특징

| 버전 | 좋았던 점 | 불편했던 점 |
|---|---|---|
| XP | RGSS 덕에 이벤트로 안 되는 것은 스크립트로 해결 가능. 이벤트 명령이 탭 형태의 버튼 목록 (UI 세부 **미확인**) | 2000/2003에 있던 이벤트 명령 일부가 삭제됨 [wikiwiki]. 공식적으로 고급 사용자 대상이라 입문 장벽이 높음. 메시지 창에 얼굴 그래픽 지원이 없었다고 알려짐 (**미확인**). Note가 없어 데이터 확장이 스크립트에 집중됨 |
| VX | XP에서 크게 줄었던 이벤트 명령을 복원해 입문자에게 쉬워짐 [wikiwiki]. 명령을 카테고리별로 정리해 찾기 쉽다는 사용자 평가가 있음 (rpgmaker.net 토론 요약). 얼굴 그래픽 지원(`Actor#face_name/face_index`) [mkxp] | 장소 이동·도주 관련 일부 명령, 타일셋 변경, 지형 태그 취득은 복원되지 않음 [wikiwiki]. 일부 명령이 빠졌다는 평가 |
| VX Ace | 타일셋 변경과 지형 태그 취득이 복원됨 [wikiwiki]. Ogg Theora 동영상 재생 명령 [窓の杜]. 파티 추종, 전투 배경 지정 등 연출 명령 증가. 전반적으로 "정리가 잘 되고 안정적"이라는 평가 | 조건 분기 항목이 조금 줄었다는 의견(스크립트 조건으로 보완). 얼굴 생성기 품질 비판 [wikiwiki] |

### 4.3 VN(비주얼노벨) 관점에서 본 한계 (세 버전 공통으로 알려진 점)

- 대사는 "문장 표시" 명령 한 개 = 창 한 페이지(몇 줄) 단위로 들어감. 대사 수백~수천 줄을 쓰기에는 **명령 단위 편집이 느림**. 텍스트 일괄 입력, 백로그, 자동 진행, 스킵, 음성 동기화 같은 기능은 기본 제공되지 않아 커뮤니티 스크립트에 의존함 (기능 부재는 통념이며 버전별 공식 문서 대조는 **미확인**).
- 선택지는 기본 최대 4개로 알려짐 (**미확인**). 그림 표시(Picture) 슬롯 수는 버전별로 다름 (정확한 수치 **미확인**).
- CG·스탠딩 일러스트는 "그림 표시/이동/회전/색조"로 연출함. 이것들이 내장 `Sprite` 속성과 1:1로 대응해서 직관적인 것은 장점임.

### 4.4 에디터 UX 교훈

- **장점**: 데이터베이스가 탭형 폼으로 한 창에 모여 있음. 테스트 전투(`System#test_battlers`, `test_troop_id`)를 에디터에서 바로 실행할 수 있음. 스위치·변수 이름을 `System`에서 관리함. 이벤트 명령은 대화상자에서 고르기만 하면 되고 파라미터 입력 검증이 있음.
- **약점**: 평면 리스트 이벤트는 길어지면 읽기 어려움 (접기, 검색, 참조 찾기가 약함). 스위치·변수가 **번호 기반 전역**이라 대형 프로젝트에서 관리가 어려움. 스크립트가 추가한 개념은 에디터에 보이지 않음.

---

## 5. 전투 시스템 차이와 커뮤니티 확장

### 5.1 기본 전투 비교

| 항목 | XP | VX | VX Ace |
|---|---|---|---|
| 시점 | 프론트뷰. 적 배틀러 + 아군 배틀러 일러스트 표시 (`Actor#battler_name`, `System#battleback_name`) [mkxp 필드, 웹] | 프론트뷰. 아군 미표시, 적 배틀러만. 전투 배경 없음 (Wikipedia: Ace에서 "재도입") | 프론트뷰. 전투 배경 2장(`battleback1/2_name`, 맵별 지정 가능) [mkxp, 窓の杜] |
| 흐름 | 턴제: 전원 커맨드 입력 후 일괄 실행. 행동 순서는 민첩 기반 (세부 **미확인**) | 턴제. `UsableItem#speed`(행동 속도 보정) 도입 [mkxp] | 턴제. `speed`, `repeats`(연속 횟수), `success_rate`, `hit_type`(필중/물리/마법) [mkxp] |
| 자원 | HP/SP | HP/MP | HP/MP/**TP** (TP 소비 스킬, 피격·행동 시 TP 획득) |
| 적 AI | `Enemy::Action`: 조건(턴, HP, 레벨, 스위치) + `rating` 가중치 [mkxp] | 조건 타입 + 파라미터 2개로 일반화 [mkxp] | 같은 구조 유지 [mkxp] |
| 전투 이벤트 | 트룹 페이지 조건(턴/적 HP/액터 HP/스위치) | `turn_ending` 조건 추가 | 동일 |

### 5.2 커뮤니티 확장이 보여주는 기본 시스템의 한계

| 확장 계열 | 사례 (출처) | 드러난 한계 |
|---|---|---|
| 사이드뷰/애니메이션 배틀러 | VX Ace: Victor's SideView, Yami SBS, Theolized SBS, Galv's Animated Battlers (Steam 토론, galvs-scripts). VX: Tankentai 계열 사이드뷰가 유명하다고 알려짐 (**미확인**, 검색으로 확인 못 함). 2003→XP에서 공식 사이드뷰가 사라졌다는 위키 언급 | 연출 계층(Sprite_Battler/Spriteset_Battle)이 "정지 그림 + 애니메이션 재생" 수준에 머물러 **행동 시퀀스(이동→모션→이펙트→데미지 팝업)를 데이터로 정의하는 방법이 없음** |
| 배틀 코어 재작성 | Yanfly Ace Battle Engine: "완전히 새 엔진은 아니지만 기본에서 주지 않던 제어권을 줌", 팝업, 적 애니메이션, 이후 애드온의 기반. 다른 스크립트가 Core Engine에 의존 (rpgmaker.net/scripts/77, Steam) | 기본 전투에 **훅과 설정 지점이 부족**해 사실상 코어를 갈아 끼워야 확장할 수 있었음 → 스크립트 간 호환성 문제 |
| ATB/CTB | VX Ace 스크립트 아카이브의 "Battle Systems (ATB, CTB, etc)" 분류 (itch.io 아카이브), VX용 사이드뷰+ATB 프로젝트 사례(검색 요약) | 턴 진행 모델이 "라운드 일괄 실행"으로 하드코딩되어 **턴 순서 정책을 교체할 수 없음** |
| HP 게이지/UI | Yanfly Enemy HP Bars 등 | 전투 UI가 Window 몇 개로 고정. 레이아웃이 코드에 박혀 있음 |
| 카드 배틀 | 공개적으로 널리 쓰인 XP/VX/Ace용 범용 카드 배틀 스크립트는 이번 조사에서 찾지 못함 (**미확인**). Pokémon Essentials(XP 기반) 쪽 Triple Triad 미니게임 사례, MZ용 Triple Triad 플러그인(코멘트: "규칙 특화, 범용 카드게임엔 위에 새로 만들어야 함") | 카드 = 별도 씬을 통째로 새로 짜야 하는 영역. 덱·손패·드로우·코스트·필드 존 같은 **카드 게임 개념이 데이터 모델에 전혀 없음** |

정리하면 기본 전투는 **"데이터(스킬/상태/적)는 잘 일반화됐지만, 진행 규칙과 연출은 코드에 하드코딩"** 되어 있음. 커뮤니티 확장의 대부분이 이 둘(턴 정책, 연출 시퀀스)을 다시 짜는 작업이었음.

---

## 6. 결론: 우리 툴에 가져올 장점 / 반복하지 말 약점

### 6.1 버전별 표

| 버전 | 가져올 장점 | 반복하지 말 약점 |
|---|---|---|
| **XP** | 내장 계층(렌더 원시 요소, 오디오, 입력)과 게임 규칙(스크립트)의 깨끗한 분리. "모든 규칙은 수정 가능한 코드"라는 철학. 적·아군 배틀러 일러스트를 보여주는 프론트뷰 (카드/일러스트 중심 게임과 잘 맞음). `Graphics.freeze/transition` 장면 전환 추상화 | 메커니즘마다 전용 필드(고정 스키마). 계수만 노출된 고정 데미지 공식. Note가 없어 데이터 확장 경로가 없음. 해상도 고정. 이벤트 명령 축소로 입문 장벽 상승 |
| **VX** | `BaseItem → UsableItem → Skill/Item` 상속 계층(공통 필드 통합). 아이콘 시트 인덱스. 얼굴 그래픽 + 메시지. `WindowVX`의 열림/닫힘, 패딩 같은 UI 원시 기능의 내장화. 해상도 가변(`resize_screen`). 이벤트 명령 카테고리 정리 | 새 기능마다 불린 필드 추가(스키마 비대화). Note 태그 문화의 시작 (에디터가 모르는 설정). 전투 배경 제거, 아군 미표시로 연출 빈약. 이전 버전 스크립트와 호환 불가 |
| **VX Ace** | **Feature(code, data_id, value) 범용 특성 리스트**를 모든 엔티티가 공유하고 런타임이 합산하는 구조. **Effect 리스트**(사용 효과). 스킬별 데미지 식. 사용자 정의 타입 목록(속성/스킬/무기/방어구 타입). TP 같은 2차 자원. 상태 해제 조건 세분화. 오디오 재생 위치 기억, 동영상 재생. 시스템 옵션 플래그 | `eval` 문자열 식(검증 없음, 오류 시 조용히 0). code가 숫자 매직넘버이고 의미가 스크립트 상수에 흩어짐. Note 태그 의존 지속. 기본 전투의 턴 정책과 연출이 하드코딩되어 코어 교체형 확장(Yanfly 등) 간 충돌. 스크립트 순서가 곧 의존성 |
| (공통) | 데이터 스키마가 런타임과 에디터에서 하나의 명세(`RPG` 모듈)로 공유됨. 테스트 전투를 에디터에서 바로 실행. BGM/BGS/ME/SE 4채널 오디오 의미 구분. 트룹(전투) 이벤트로 전투 중 스토리 삽입 | 평면 리스트 이벤트(긴 VN 스크립트 작성에 부적합). 번호 기반 전역 스위치·변수. 바이너리(Marshal+zlib) 저장이라 diff와 VCS가 어려움. 에디터를 확장할 수 없음 |

### 6.2 우리 툴 설계 제언 (카드 RPG + VN 기준)

1. **데이터 모델은 VX Ace를 뼈대로**: `Card`(=UsableItem)가 `cost`, `type`(사용자 정의 열거형), `effects[]`, `damage{formula}`를 가짐. 패시브·장비·버프·캐릭터 고유 능력은 모두 `features[]`. Feature/Effect code는 숫자가 아니라 **이름 있는 레지스트리**(설명, 파라미터 스키마, 집계 규칙 포함)로 두고 에디터 폼을 자동 생성함.
2. **식은 eval 대신 검증 가능한 DSL**로: 변수 자동완성, 문법 오류 표시, 샘플 a/b로 계산 미리보기.
3. **Note 태그 대신 커스텀 필드 스키마**: 프로젝트별로 필드를 정의하면 에디터 UI에 바로 나타나게 함.
4. **전투 코어에 교체 가능한 정책 지점**을 둠: 턴 순서(라운드/ATB/CTB), 드로우·손패·덱 존, 타겟 선택, 행동 시퀀스(연출)를 데이터로 정의. 커뮤니티가 코어를 갈아엎어야 했던 부분을 처음부터 열어둠.
5. **XP식 내장/규칙 분리 + 명시적 확장**: 런타임 코어(Sprite/Window/Audio/Transition 상당)는 고정 API로 두고, 게임 규칙은 모듈 단위 + 훅으로 확장. 순서 의존 대신 의존성 선언.
6. **스토리는 이벤트 리스트와 별도의 텍스트 우선 스크립트 형식**: 대사 일괄 작성, 화자·표정·CG·BGM 지시를 인라인으로 씀. 백로그, 스킵, 자동 진행은 기본 제공. 전투 중 컷인은 트룹 이벤트처럼 "전투 조건 → 스토리 호출" 훅으로 연결.
7. **저장은 텍스트(JSON 등) + 이름 기반 ID**: 스위치·변수는 이름과 네임스페이스로 관리하고 참조 찾기를 지원함. 테스트 전투와 장면 단위 즉시 실행 기능은 RPG Maker처럼 유지함.

### 6.3 미확인 항목 요약

- RGSS2의 정확한 내장 Ruby 버전 (1.8.1 대 1.8.3), VX의 해상도 상한, VX Ace 640x480 상한의 공식 근거
- XP 속성·상태 A~F 등급별 배율, 기본 데미지 공식, 행동 순서 공식 (기본 스크립트 내용이라 미열람)
- VX Ace Feature/Effect code 번호와 의미의 대응표, 공식 탭 항목명 전체 (MV Traits와 같은 계열로 추정)
- 버전별 선택지 최대 수, Picture 슬롯 수, XP 메시지 얼굴 그래픽 지원 여부, 스크립트 에디터 기능 세부
- VX Tankentai 사이드뷰의 세부, XP/VX/Ace용 범용 카드 배틀 스크립트의 존재 여부
- 공식 제품 페이지 본문 (WebFetch 파싱 오류로 검색 요약만 확인)
