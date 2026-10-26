# 06. RPG Maker MZ 구조·MV 대비 변경점 및 시리즈 에디터 UX 분석

- 분석일: 2026-10-26
- 대상: RPG Maker MZ(이하 MZ), 비교 대상 RPG Maker MV(이하 MV) 및 시리즈 전체 에디터
- 목적: 1인 개발 2D 카드 수집형 턴제 RPG + 비주얼노벨식 스토리(대사·CG·BGM·SE), Android 출시용 **전용 제작 툴**의 에디터 UX 설계 참고
- 범위 제외: 타일맵·필드 이동(언급은 하되 결론에서 제외)

## 0. 출처 및 조사 방법

### 자료 원칙
- MZ 코어 스크립트·에디터 바이너리는 **받지도, 열어 보지도, 디컴파일하지도 않았음**. MZ의 내부 구현 설명은 전부 공식 웹 도움말, 공개 문서, 커뮤니티 글에 근거함.
- MV는 로컬에 있는 MIT 공개 소스(`engine-tool-refs/rpgmaker-mv-corescript`)를 비교용으로만 참고함.
- 조사 환경에서 일부 사이트는 네트워크 차단이나 파싱 오류로 직접 열지 못함: `rpgmakerweb.com` 블로그, `tkool.jp`(공식 플러그인 작성 문서), `developer.rpgmakerweb.com`(공식 API 문서), `forums.rpgmakerweb.com`, `hackmd.io`, `rpgmaker.net`. 이 사이트들의 내용은 검색 결과 요약으로만 확인했고, 본문에서 **[검색요약]**으로 표시함.
- 1차 출처로 재확인하지 못한 내용은 **미확인**, 일반적으로 알려져 있으나 이번 조사에서 출처를 다시 확인하지 못한 내용은 **[일반지식·미확인]**으로 표시함.

### 출처 목록

**공식 (Gotcha Gotcha Games / KADOKAWA)**
1. MZ 도움말 – Main Features of RPG Maker MZ: https://rpgmakerofficial.com/product/MZ_help-en/01_01_01_01.html
2. MZ 도움말 – MV Data Conversion: https://rpgmakerofficial.com/product/MZ_help-en/01_01_01_02.html
3. MZ 도움말 – Time Progression Battles: https://rpgmakerofficial.com/product/MZ_help-en/01_01_01_03.html
4. MZ 도움말 – Animations: https://rpgmakerofficial.com/product/MZ_help-en/01_01_01_04.html
5. MZ 도움말 – Improvements to Artistic Expressions: https://rpgmakerofficial.com/product/MZ_help-en/01_01_01_05.html
6. MZ 도움말 – Game Control Improvements: https://rpgmakerofficial.com/product/MZ_help-en/01_01_01_06.html
7. MZ 도움말 – System Requirements: https://rpgmakerofficial.com/product/MZ_help-en/01_01_02.html
8. MZ 도움말 – Basic Editor Controls: https://rpgmakerofficial.com/product/MZ_help-en/01_03.html
9. MZ 도움말 – Menu Bar Content: https://rpgmakerofficial.com/product/MZ_help-en/01_04.html
10. MZ 도움말 – How to Use Aid Tools (플러그인 관리자, 사운드 테스트, 이벤트 검색기, 캐릭터 생성기, 테스트 플레이, 리소스 관리자, 옵션): https://rpgmakerofficial.com/product/MZ_help-en/01_05.html
11. MZ 도움말 – Playing Your Game (조작, F2~F9 키): https://rpgmakerofficial.com/product/MZ_help-en/01_06.html
12. MZ 도움말 – Database: https://rpgmakerofficial.com/product/MZ_help-en/01_08.html
13. MZ 도움말 – Troop Settings (전투 테스트): https://rpgmakerofficial.com/product/MZ_help-en/01_08_07.html
14. MZ 도움말 – Animation Settings / Effekseer: https://rpgmakerofficial.com/product/MZ_help-en/01_08_09.html , https://rpgmakerofficial.com/product/MZ_help-en/01_08_09_01.html
15. MZ 도움말 – System 1 / System 2: https://rpgmakerofficial.com/product/MZ_help-en/01_08_12_01.html , https://rpgmakerofficial.com/product/MZ_help-en/01_08_12_02.html
16. MZ 도움말 – Map Event Creation and Controls: https://rpgmakerofficial.com/product/MZ_help-en/01_09_02.html
17. MZ 도움말 – Event Commands 목록 / Messages(Show Text) / Picture / Advanced(Plugin Command): https://rpgmakerofficial.com/product/MZ_help-en/01_10.html , https://rpgmakerofficial.com/product/MZ_help-en/01_10_01.html , https://rpgmakerofficial.com/product/MZ_help-en/01_10_08.html , https://rpgmakerofficial.com/product/MZ_help-en/01_10_16.html
18. MZ 도움말 – Output Formats (배포): https://rpgmakerofficial.com/product/MZ_help-en/01_11_03.html
19. MZ 도움말 – Enabling Plugins / Using Official Plugins: https://rpgmakerofficial.com/product/MZ_help-en/01_11_04.html , https://rpgmakerofficial.com/product/MZ_help-en/01_11_05.html
20. MZ 도움말 목차 데이터(RoboHelp TOC): https://rpgmakerofficial.com/product/MZ_help-en/whxdata/toc.new.js (API 문서 링크 https://developer.rpgmakerweb.com/rpg-maker-mz/ 포함, 본문은 차단으로 미열람)
21. MV 도움말 – Plugin Specifications: https://rpgmakerofficial.com/product/MV_Help/page/01_11_03.html
22. MV 도움말 – Output Formats: https://rpgmakerofficial.com/product/MV_Help/page/01_11_04.html
23. MV 도움말 – Converting to an Android App (Crosswalk): https://rpgmakerofficial.com/product/MV_Help/page/01_11_05.html
24. 공식 블로그 – MZ Preview #2 (그래픽·매핑·이벤트) [검색요약]: https://rpgmakerweb.com/blog/rpg-maker-mz-preview-2-graphics-mapping-eventing
25. 공식 블로그 – MZ Preview #3 (캐릭터 생성기·플러그인 관리자·플러그인 커맨드) [검색요약]: https://www.rpgmakerweb.com/blog/rpg-maker-mz-preview-3-character-generator-plugin-manager-event-plugin-command
26. 공식 블로그 – Making MV Style Animations (1.4.0) [검색요약]: https://www.rpgmakerweb.com/blog/making-mv-style-animations
27. 공식 블로그 – MZ v1.1.0 update [검색요약]: https://www.rpgmakerweb.com/blog/rpg-maker-mz-v1-1-0-update
28. 공식 커뮤니티(RPG Maker Guild) – MZ vs MV 질문 스레드: https://guild.rpgmakerofficial.com/t/topic/2290?tl=en
29. 공식 커뮤니티(RPG Maker Guild) – MZ 픽셀 공격 애니메이션 [검색요약]: https://guild.rpgmakerofficial.com/t/topic/2117

**위키·커뮤니티·리뷰**
30. Yanfly Wiki – RMMZ Changelog (rmmz_*.js 파일별 변경 기록) [검색요약]: https://www.yanfly.moe/wiki/RMMZ_Changelog
31. Indie Hell Zone – My Impressions of RPG Maker MZ (2020-08): https://indiehellzone.com/2020/08/25/my-impressions-of-rpg-maker-mz/
32. GameFromScratch – RPGMaker MZ Released [검색요약]: https://gamefromscratch.com/rpgmaker-mz-released/
33. rpgmaker.net – "What can be done to make RMMZ have overwhelming positive reviews?" [검색요약]: https://rpgmaker.net/forums/topics/25423
34. Reddit r/RPGMaker – "Is MZ worth it" (미러) [검색요약]: https://lr.in.psf.lt/r/RPGMaker/comments/vk9yqh/is_mz_worth_it
35. Steam 커뮤니티 – MZ 토론 [검색요약]: https://steamcommunity.com/app/1096900/discussions/0/4846392474897321059 , https://steamcommunity.com/app/1096900/discussions/0/3017941618716258551
36. SteamDB – MZ 1.8.0 패치노트 [검색요약]: https://steamdb.info/patchnotes/13050809
37. itch.io – Pretty JSON MV+MZ (Git 병합용 데이터 포맷 플러그인, 댓글에 "JSON Format Level" 언급) [검색요약]: https://tamschi.itch.io/pretty-json-for-rpg-maker
38. itch.io – Postmortem #5 Learning MZ [검색요약]: https://itch.io/devlog/507118/postmortem-5-learning-mz.amp
39. Effekseer 고급 가이드(블로그) [검색요약]: https://elizzyviolet.mataroa.blog/blog/the-only-semi-advanced-effekseer-guide-for-rpg-maker-mz/
40. Wavedash 문서 – RPG Maker MZ 웹 배포: https://docs.wavedash.com/engines/rpgmaker.md
41. HackMD – RPGMakerMVMZ Android (Android Studio 래퍼 가이드) [검색요약]: https://hackmd.io/@Mirai/RPGMakerMVMZ-Android_eng
42. itch.io – Mobile Exporter for RPG Maker (Capacitor 프로젝트 생성기) [검색요약]: https://mindoro-evo.itch.io/mobile-exporter-for-rpg-maker
43. itch.io – APK Converter (Cordova 기반) [검색요약]: https://tendev2d.itch.io/apk-converter
44. Scribd – Build for Android (Cordova 가이드, 신뢰도 미검증) [검색요약]: https://www.scribd.com/document/871979542/Build-for-Android
45. itch.io – MZ 게임의 Android 성능 관련 댓글 묶음 [검색요약]: https://itch.io/post/9955732 , https://itch.io/post/9962914
46. itch.io – Plugin Builder for MV/MZ (플러그인 헤더 GUI 생성기) [검색요약]: https://mindoro-evo.itch.io/plugin-builder-for-mvmz
47. Effekseer 공식: https://effekseer.github.io/en/

**로컬 참고 (MIT)**
48. `C:\Users\DKSYSTEMS\Desktop\개인\engine-tool-refs\rpgmaker-mv-corescript` (`js/rpg_objects/Game_Interpreter.js`의 `command356`/`pluginCommand`, `js/rpg_managers/PluginManager.js`, `js/libs/*`, `package.json`의 concat 빌드 구조)

---

## 1. 요약 (TL;DR)

- MZ는 구조적으로 **"MV 2"**에 가깝다. JS/HTML5 런타임, JSON 데이터, `Scene → Window/Sprite → Game_* → Manager` 계층, 노트태그 메타데이터는 그대로 유지된다. 큰 변화는 다음과 같다. (1) 플러그인 커맨드와 파라미터를 **어노테이션 기반의 구조화된 GUI**로 다룬다. (2) **TPB**(시간 진행 배틀) 옵션이 생겼다. (3) 애니메이션이 **Effekseer**로 바뀌었다. (4) 맵 레이어를 고를 수 있게 됐다. (5) 터치 UI가 기본 탑재됐다. (6) 해상도·폰트·게임 ID를 에디터에서 설정한다.
- 에디터 UX의 핵심 강점은 **"폼 기반 데이터베이스 + 명령 리스트형 이벤트 에디터 + 즉시 테스트"** 3종 세트다. 반면 사용자들이 꾸준히 지적하는 약점은 모달 창 중첩, 실행 취소 범위 제한(공식 기준 20단계, 맵/이벤트 중심), 텍스트 diff·병합이 어려운 데이터, MV 플러그인 비호환, 외부 리소스 반입의 번거로움이다.
- 모바일 배포는 공식적으로 **웹 출력까지만** 지원한다. APK는 커뮤니티가 Cordova, Capacitor, Android Studio WebView로 직접 감싸서 만든다. 성능, 오디오, IAP, 서명 문제는 개발자 몫이다.
- 우리 툴에 가져올 뼈대는 **DB 탭 + 리스트/상세 2단 폼**, **명령 리스트형 시나리오 에디터**, **어노테이션→자동 인스펙터**, **엔티티 단위 즉시 테스트(전투 테스트·대사 미리보기)**다. 여기에 Unity의 인스펙터·프리팹·드래그앤드롭·Play 모드, Godot의 텍스트 리소스(.tres)·시그널을 더해 **모달 제거, 무제한 Undo, Git 친화 데이터, 핫리로드**를 확보하는 것이 핵심이다.

---

## 2. MZ의 MV 대비 변경점

### 2.1 런타임 구조

| 항목 | MV | MZ | 근거 |
|---|---|---|---|
| 언어/플랫폼 | JavaScript + HTML5, NW.js(데스크톱) | 동일 | [출처 18, 22] |
| 코어 파일 | `rpg_core.js`, `rpg_managers.js`, `rpg_objects.js`, `rpg_scenes.js`, `rpg_sprites.js`, `rpg_windows.js` (개발 소스는 클래스별 파일로 나뉘어 있고 `concat.js`로 6개 파일로 합침) | `rmmz_core.js`, `rmmz_managers.js`, `rmmz_objects.js`, `rmmz_scenes.js`, `rmmz_sprites.js`, `rmmz_windows.js`. **계층 분할은 같고 접두사만 바뀜** | MV: 로컬 `package.json`. MZ: Yanfly RMMZ Changelog의 파일별 기록 [30], 사용자 오류 로그 [검색요약] |
| "파일 구성 통합" | 배포본은 6개 파일 + `libs/`(pixi, pixi-tilemap, pixi-picture, fpsmeter, lz-string, iphone-inline-video) | 공식 자료에서 "파일 통합"을 명시한 문장은 찾지 못함. 커뮤니티에서는 pixi-tilemap/pixi-picture 등 외부 라이브러리 의존을 줄이고 코어에 흡수했다고 알려져 있음 | **[일반지식·미확인]**. MZ 코어를 직접 보지 않는다는 원칙에 따라 검증하지 않음 |
| 렌더러 | PIXI v4, WebGL/Canvas 둘 다 | **WebGL 전용**("Can exclusively be played on browsers that support WebGL") | [18]. PIXI 버전(v5)은 **[일반지식·미확인]** |
| 프로젝트 파일 | `Game.rpgproject` | `game.rmmzproject` | [9], 포럼 [검색요약] |
| 기본 해상도 | 816×624 | 816×624, **System 2에서 화면/UI 영역 크기 설정 가능** | [15], [40] |
| 폰트 | `fonts/gamefont.css` 수동 수정 | DB System 2에서 메인·숫자·폴백 폰트와 폰트 크기 설정 | [2], [15] |
| 오디오 | ogg + m4a 두 벌 | MV→MZ 변환표상 audio 폴더는 그대로 복사. MZ가 ogg만 쓴다는 점은 **[일반지식·미확인]** | [2] |
| 애니메이션 데이터 | `img/animations` 셀 시트 + Animations.json | `effects/` 폴더의 Effekseer `.efkefc` + Animations.json. MV 애니메이션 변환은 "편집 불가로 비권장"이었다가 1.4.0부터 MV 호환 애니메이션 작성 지원 | [2], [14], [26] |
| 코어 버전 관리 | 코어 JS를 수동으로 덮어씀 | 메뉴 **[Game] → [Update Core Script]**로 업/다운그레이드 | [5], [9] |
| 세이브 | 로컬 파일 또는 localStorage | **오토세이브**(전투 종료, 장소 이동 시), **Game ID**로 브라우저 배포판 업데이트 시 세이브 이전 | [6], [15] |
| 오류 처리 | 크래시 시 중단 | 사용자 경험담: "MZ는 캐시·DB 한도가 커졌고 크래시 핸들러가 랙 스파이크 후 계속 실행" | [28] (개인 경험담) |

**설계 시사점**: MZ도 런타임 계층 분할(Core/Managers/Objects/Scenes/Sprites/Windows)은 MV와 같다. 리팩터링보다는 **에디터 측 기능(해상도·폰트·코어 버전·Game ID)을 GUI로 끌어올린 것**이 MZ 변화의 본질이다. 우리 툴도 "런타임 설정 = 에디터의 프로젝트 설정 화면"으로 일원화하는 편이 좋다.

### 2.2 플러그인 커맨드·파라미터 구조화

**MV 방식** (로컬 소스로 확인):
```js
// Game_Interpreter.js (MV, MIT)
Game_Interpreter.prototype.command356 = function() {
    var args = this._params[0].split(" ");   // 한 줄 문자열을 공백으로 분리
    var command = args.shift();
    this.pluginCommand(command, args);       // 모든 플러그인이 이 메서드를 체인 오버라이드
};
```
- 사용자는 `MyPlugin show 3` 같은 **자유 텍스트**를 입력함. 오타를 검증하지 않고, 인자는 전부 문자열이며, 플러그인끼리 같은 커맨드 이름을 쓰면 충돌함.
- 파라미터 어노테이션은 MV 도움말 공식 문서 기준으로 `@plugindesc`, `@author`, `@param`, `@desc`, `@default`, `@help`, 언어 블록 `/*:ja`. "All parameters will be treated as strings" [21]. MV 1.5 이후 `@type`, `@parent`, struct 등이 추가된 것은 **[일반지식·미확인]**(공식 페이지에는 기본 태그만 있음).

**MZ 방식**:
- 이벤트 커맨드 [Plugin Command]에서 **플러그인 → 커맨드 이름 → 인자**를 드롭다운과 폼으로 고른다. 공식 표현은 "Specify the plugin (enabled in the Plugin Manager), choose the command name, and set any arguments the plugin defines" [17-Advanced]. 리뷰도 "free-text entry를 드롭다운으로 대체"라고 정리한다 [검색요약, 32].
- 플러그인 헤더 어노테이션의 `@command`/`@arg`로 커맨드와 인자를 선언한다. 서드파티 도구들이 `js/plugins/*.js`의 `@command`/`@arg`를 파싱해 검증한다는 점에서 이 태그가 사실상 표준임을 확인할 수 있다 [검색요약: glama rpgmaker-mz-mcp, 46].
- 플러그인 관리자는 **로드 순서와 의존성(base plugin)을 선언받아 경고**한다. 공식 Preview #3 요지: "creators can define load orders and requirements… The Plugin List will then tell you if you don't have things set up right" [25, 검색요약]. 태그 이름은 `@base`/`@orderAfter`/`@orderBefore`로 알려져 있음 **[일반지식·미확인: 공식 작성 문서 tkool.jp 차단]**.
- 파라미터 타입은 `@type struct<Name>`과 배열 접미사 `[]`를 쓴 실제 플러그인 예가 확인됨 [검색요약]. 그 밖에 `number/string/multiline_string/file/boolean/select/combo/actor/skill/switch/variable…`, `@min/@max/@decimals/@option/@value/@dir/@parent`, 미사용 파일 제외용 `@require`/`@noteParam`류 등 **세부 목록은 미확인**(공식 문서 직접 열람 실패).
- 런타임 등록은 `PluginManager.registerCommand(pluginName, commandName, func)`로 알려져 있음 **[일반지식·미확인]**. 스크립트에서 `PluginManager.callCommand`로 호출하는 방법은 Qiita 글에서 언급됨 [검색요약].
- 플러그인 관리자 편의 기능 [10, 19]: 목록에서 체크박스로 ON/OFF, Ctrl+F 플러그인 검색, 플러그인 도움말 검색, 파라미터 입력 중 우클릭 [Insert Icon Number](아이콘셋 뷰어), [File]/[Text] 탭, js 교체 후 **[Update]로 파라미터 정의 갱신**(Shift로 여러 개 선택).
- 공식 동봉 플러그인 중 **PluginCommonBase**(파라미터·커맨드·노트에서 `\v[n]` 제어문자), **TextScriptBase**(여러 줄 텍스트/스크립트 DB), **MaterialBase**(플러그인이 쓰는 리소스를 등록해 미사용 파일 제외에서 보호), **UniqueDataLoading**(임의 JSON을 data에서 로드), **ButtonPicture/TextPicture**, **EventCommandByCode**가 눈에 띈다 [19]. 이는 "코어 DB가 못 다루는 데이터"를 플러그인으로 보완하는 구조가 공식화된 것이다.

**알려진 불편** [검색요약]: 파라미터 설정을 개별 항목 단위로 복사/붙여넣기 할 수 없고 플러그인 단위로만 가능함. 파라미터가 많은 플러그인은 struct 안의 struct로 대화상자가 계속 중첩됨. 서드파티 저작 도구 중 일부는 "structs still have errors"라고 경고함. MV 플러그인은 MZ에서 동작이 보장되지 않음 [2: "Plugins are not guaranteed to run correctly or at all"], [31: "throws a bunch of resources out the window"].

**설계 시사점**: 우리 툴에서는 **스키마(어노테이션 또는 타입 정의) → 인스펙터 자동 생성 → 시나리오 명령 폼 자동 생성**을 같은 메커니즘으로 처리한다. 모든 인자는 **타입 있는 값**으로 직렬화하고, 런타임에서 문자열 파싱을 없앤다. MZ의 struct 중첩 모달 대신 Unity/Godot식 **접이식 인라인 인스펙터**를 쓴다.

### 2.3 전투: 턴제 vs TPB, 사이드뷰

- System 1 → [Battle System]에서 선택 [15]:
  - **Turn-Based**: 모든 배틀러가 한꺼번에 행동함(MV 방식).
  - **Time Progression (Active)**: 민첩에 따라 게이지가 차고, 커맨드를 입력하는 동안에도 시간이 흐름.
  - **Time Progression (Wait)**: 커맨드를 입력하는 동안 시간이 멈춤.
- TPB 설명 [3]: 액터마다 게이지가 표시되고, 100%가 되면 행동한 뒤 0으로 리셋됨. 도움말 원문에는 "TP gauge"라고 적혀 있는데 문맥상 시간 게이지임.
- 화면 시점: System 1 → [Battle Screen]의 Front-view/Side-view. 사이드뷰 공격 모션, 마법 스킬 영창 모션 매핑은 System 2 [15]. 사이드뷰 자체는 MV에서 이미 지원함(MV 신기능).
- 스킬/아이템 개선 [5]: **Scope 전용 대화상자**, 효과 "공통 이벤트" 여러 개 실행, 특성 "Attack Skill"(평타를 다른 스킬로 교체), 트룹 화면에서 적 ID 표시, 전투에서 터치로 대상 선택 [6].
- 이벤트 변수 피연산자 **"Just Prior"**(직전 행동 정보 등). 예: 특정 스킬로 죽었을 때 다른 대사 출력 [5].
- 데미지 공식은 `a.atk * 4 - b.def * 2` 같은 **문자열 JS 식**을 유지함 [17-Skill].
- 커뮤니티 평가: TPB는 "행동 바가 차면 행동, Active는 고르는 동안에도 찬다"는 설명 수준이며, 심층 평가 자료는 찾지 못함(**미확인**).

**설계 시사점(카드 RPG)**: 우리 게임은 턴제 카드 전투이므로 TPB를 그대로 가져올 필요는 없다. 다만 **"전투 진행 규칙을 DB 시스템 설정 하나로 교체"**하는 설계(전략 패턴)는 유효하다. 예: 덱/코스트 규칙, 선공 결정 방식. 데미지 공식 문자열은 편리하지만 오타 검증이 없으므로, 우리 툴은 **식 에디터 + 미리보기 계산기**(공격자/대상 샘플 값으로 즉시 결과 표시)를 제공한다.

### 2.4 Effekseer 애니메이션

- 공식 지원 버전은 Effekseer 1.5. `.efkefc`를 프로젝트의 `effects/` 폴더에 넣으면 DB → Animations → [Particle Effect]에서 고를 수 있음 [14].
- 애니메이션 DB 항목 [14]: 표시 타입, 파일, Scale/Speed/Rotation/Offset, **미리보기(Play, 대상 이미지 변경, 전투 배경 변경)**, 프레임별 SE 타이밍, 프레임별 플래시(색·지속 시간).
- 1.4.0부터 MV 방식 셀 애니메이션 작성 지원(192×192 셀). DB에서 우클릭 "create MV compatible data" [26, 검색요약].
- 평가: "가장 의미 있는 변화, 결과물이 더 멋짐" vs "Effekseer를 새로 배워야 함" [31]. MV 애니메이션 이식이 어렵다는 불만 [33]. 일부 3D 모델 컬링 문제 보고 [39]. MZ 구버전과 신규 Effekseer 팩의 비호환, 상대 경로 문제 [38]. "픽셀 퍼펙트도 가능해 불만 해소"라는 반론도 있음 [38].

**설계 시사점**: 우리는 카드 연출(공격·버프·필살기 컷인) 위주다. 외부 파티클 툴에 의존하기보다 **타임라인형 연출 에디터**(스프라이트·셰이크·플래시·SE·카메라 키프레임)를 내장하는 편이 1인 개발 범위에 맞다. 가져올 점은 MZ의 **"프레임별 SE/플래시 트랙 + 대상·배경을 바꿔 가며 미리보기"** 구성이다.

### 2.5 맵 레이어 (참고만, 우리 범위 밖)

- MV는 자동 레이어만 지원했다. MZ는 **레이어를 골라 개별 편집**할 수 있고(툴바 Layer 버튼, [Layer] 메뉴), 자동 레이어도 유지했다 [5, 8, 9].
- 그 밖에 맵 트리 재정렬, 이동 경로 미리보기, 장소 이동 시 좌표 표시, 풍선 아이콘 미리보기, 던전 자동 생성이 추가됐다 [5, 10, 검색요약].
- 불만: "현재 레이어를 보여 주는 시각 표시가 약함" [31]. 48px 타일은 논쟁거리 [31].

**설계 시사점**: 타일맵은 제외하지만, **"현재 편집 컨텍스트(레이어/모드)를 항상 눈에 띄게 표시"**하라는 교훈은 CG/스탠딩 레이어 편집에도 적용된다.

### 2.6 모바일·터치 UI

- 공식 [6]: 스마트폰 UI 개선, PC와 스마트폰 겸용 메뉴, 전투 중 터치로 대상 선택, 터치 대응 공식 플러그인.
- 조작 표 [11]: 탭은 결정, **두 손가락 탭은 취소/메뉴**, 길게 눌러 스와이프는 페이지 스크롤, 화면 내 취소/메뉴 버튼. 조건 분기에 터치/마우스 조건(길게 누르기 등) 추가 [5].
- 상점 수량 입력도 터치 버튼을 지원함 [11].
- 기본 스트레치 모드: 커뮤니티 문서에 따르면 MZ는 기본적으로 **모바일 또는 nwjs에서만 캔버스를 늘리고**, 데스크톱 웹에서는 816×624로 고정됨 [40].

**설계 시사점**: MZ의 터치 대응은 **키보드/패드 중심 UI를 터치로 이식**한 것이다(두 손가락 탭 취소 등은 발견성이 낮음). 우리 게임은 처음부터 터치를 1순위로 두고, 에디터에 **해상도·세이프에어리어·종횡비 프리뷰**(Unity Game 뷰의 해상도 드롭다운처럼)를 넣는다.

### 2.7 성능

- 공식 최소 사양(에디터) [7]: Win 8.1/10 64bit 또는 macOS 10.13+, i3-4340급, RAM 8GB, VRAM 1GB(OpenGL). 플레이 사양은 "게임 내용에 따라 다름".
- 웹/모바일 플레이 요건 [18]: HTML5 Canvas + Web Audio, **iOS 12+, Android 8+**(MV는 iOS 8+, Android 4.4+ [22]), WebGL 필수, 스마트폰 브라우저는 Wi-Fi 권장.
- 커뮤니티 평가: "그래픽 위주 작업(애니메이션·특수효과·조명 플러그인)에서는 MZ가 몇 배 빠르다" [34, 일화], "스크립트 호출 줄 수 제한 완화" [34]. 반면 "단순한 게임도 요구 사양이 높다", "MV보다 모바일 이식이 어렵다", JoiPlay에서 8fps 사례 [45, 일화].
- **정량 벤치마크는 공개 자료에서 찾지 못함(미확인).**

---

## 3. 에디터 UX (시리즈 전체 비교)

### 3.1 시리즈 흐름 개관 **[일반지식·미확인. 이번 조사에서 1차 출처 재확인 안 함]**

| 세대 | 스크립트 | 에디터 특징 요약 |
|---|---|---|
| 2000/2003 | 없음(이벤트만) | DB 탭 + 이벤트 명령 리스트 구조 확립. 2003은 사이드뷰 ATB 전투 |
| XP | Ruby(RGSS1), 내장 스크립트 에디터 | 3레이어 맵, 코어 스크립트를 에디터 안에서 직접 편집 |
| VX / VX Ace | RGSS2/3 | 자동 레이어, 단순화. Ace에서 특성(Traits)·효과(Effects) 체계 도입 |
| MV | JavaScript | 내장 스크립트 에디터 폐지 → **플러그인 관리자**. 사이드뷰, 멀티플랫폼 배포 |
| MZ | JavaScript | 플러그인 커맨드 GUI화, TPB, Effekseer, 레이어 선택, 각종 미리보기 |

공통 뼈대는 20년 넘게 유지된 **"메인 창(맵) + 모달 DB 창 + 모달 이벤트 에디터"**다. 코드 확장 방식은 **"에디터 내장 스크립트(XP~Ace) → 외부 플러그인 파일 + 어노테이션 GUI(MV~MZ)"**로 바뀌었다.

### 3.2 데이터베이스 탭 화면 구성 [12]

- **좌측 세로 탭**(Actors, Classes, Skills, Items, Weapons, Armors, Enemies, Troops, States, Animations, Tilesets, Common Events, System 1, System 2, Types, Terms) → **가운데 ID 리스트** → **우측 상세 폼**의 3단 구성.
- 리스트: `0001: 이름` 형태의 고정 ID. [Change Maximum]으로 최대 개수 조절(스킬·아이템·무기·적·트룹은 최대 2000, 그 외 1000). 최대치를 줄이면 넘는 데이터는 삭제됨.
- 우클릭: Copy(Shift로 여러 개), Paste(**덮어쓰기**), Clear. 우클릭 [Search]와 Search Next/Previous [8].
- **Notes** 필드: `<name:data>` 형식 메타데이터 → `object.meta.name` [21]. 플러그인 확장의 핵심 통로이지만 자유 텍스트라서 검증되지 않음.
- Traits/Effects: 리스트 박스에서 더블클릭 → 별도 모달 창 [8].
- Object Selector 옵션: 아이템/무기 선택을 Dropdown, Extended(별도 창), Smart(개수에 따라 자동) 중에서 고름 [10].
- 숫자 필드에 전각 숫자를 입력하면 반각으로 자동 변환 [5]. Ctrl+Enter로 OK [17-Show Text].
- OK/Cancel/Apply 버튼 방식의 **트랜잭션형 편집** [8].

**장점**: 한 화면에서 모든 정의를 찾을 수 있고, ID 기반이라 이벤트·식에서 참조하기 쉬움. 비프로그래머가 이해하기 쉬운 폼 구성.
**불편**: 탭 종류가 고정되어 있어 새 데이터 종류(우리의 카드, 덱, 시나리오 챕터 등)를 추가할 수 없고 노트 태그나 플러그인 JSON(UniqueDataLoading)으로 우회해야 함. 덮어쓰기 붙여넣기, ID 재정렬 불가, 모달 Traits 창. DB 쪽 Undo 범위는 공식 자료에서 확인하지 못함(**미확인**. Edit 메뉴의 Undo 20회는 맵·이벤트 대상으로 기술됨 [9]).

### 3.3 이벤트 에디터 (명령 리스트 + 명령 선택 창)

- 맵 이벤트는 더블클릭으로 생성 → **Event Editor 창**(페이지 탭, 출현 조건, 이미지, 트리거, Contents 명령 리스트) [16].
- **명령 리스트**: `◆` 줄 단위의 들여쓰기 트리(조건 분기·루프가 블록을 이룸). 빈 줄을 더블클릭하면 **명령 선택 창**이 열림. 3개 탭(Tab 1: 메시지/진행/흐름/파티/액터, Tab 2: 이동/캐릭터/픽처/타이밍/화면/오디오, Tab 3: 씬/시스템/맵/전투/고급)에 카테고리별 버튼이 배치됨 [17].
- 명령마다 **전용 모달 폼**이 열림. MZ에서 추가된 미리보기: Show Text 미리보기, Set Movement Route 미리보기, 풍선 미리보기, Show/Move Picture 빠른 설정(1.8.0: X/Y 직접 입력, 화살표 이동, Shift 스냅, Ctrl 1px) [5, 36].
- Quick Event Creation(이동/문/보물/여관 템플릿) [16]. Event Searcher(스위치/변수/이름으로 이벤트 검색) [10].
- 배틀 이벤트(트룹): 페이지별 조건(턴 종료/턴 수/적 HP/액터 HP/스위치) + Span(Battle/Turn/Moment) [13].
- 커뮤니티: "MV와 MZ의 이벤트 에디터는 거의 같다" vs "이동 경로·픽처·풍선 미리보기와 픽처 이징이 업그레이드할 가치" [28]. "게임 체인저는 없다" [31].

**장점**: 비프로그래머도 **절차를 위에서 아래로 읽을 수 있는** 선형 스크립트. 명령마다 폼이 있어 잘못된 입력이 줄어듦. 복사/붙여넣기와 Shift 다중 선택. 공통 이벤트로 재사용.
**불편(포럼·리뷰 및 구조 분석)**: 명령마다 모달 창이 열려 클릭 수가 많음. 긴 대사 시나리오는 Show Text 블록이 수백 줄로 길어짐(Batch Entry로 4줄씩 자동 분할하는 정도만 지원 [17-Show Text]). 리스트 안에서 텍스트 검색·치환하는 공식 기능은 확인하지 못함(**미확인**). 서드파티 비주얼 이벤트 에디터가 데이터를 망가뜨린 사례 [검색요약]. 라벨/점프 외에는 구조화 수단이 약함.

### 3.4 문자(대사) 미리보기 [17-Show Text]

- 얼굴 이미지, **Name**(MZ 신규: 화자 이름 박스), 배경(창/어둡게/투명), 위치(하/중/상), **[Preview] 버튼**.
- 텍스트 칸의 **세로선 가이드**로 창 최대 폭을 표시. 우클릭으로 [Insert Color Number]/[Insert Icon Number]/[Plugin Help].
- 제어문자 `\V \N \P \G \C \I \{ \} \\` + MZ 신규 `\PX \PY \FS` [5].
- Batch Entry: 4줄을 넘는 텍스트를 Show Text 여러 개로 자동 분할.

**VN 관점 평가**: 대사 1개 단위 미리보기는 좋지만, **시퀀스 전체를 연속 재생**하는 미리보기(스탠딩 CG, BGM, 연출 포함)는 테스트 플레이에서만 가능하다. 스탠딩 일러스트는 Show Picture로 직접 조립해야 하고, 공식 Name 필드에는 화자 DB 연동이 없다(**구조 분석 기반, 플러그인 생태계에 대안 다수**).

### 3.5 리소스 관리자 [10]

- 좌측 폴더 리스트, 우측 파일 리스트, **이미지 미리보기**(사운드는 Sound Test에서 따로 들어야 함).
- Import(여러 개 동시 가능)/Export/Delete(복구 불가)/DLC 가져오기.
- 자산 규격(Asset Standards)을 지키지 않으면 쓸 수 없음. 파일명이 곧 참조 키라서 이름을 바꾸면 참조가 깨짐 **[구조 분석, 미확인]**.
- 배포 시 **[Exclude Unused Files]**: 플러그인이 쓰는 파일은 MaterialBase 등록이나 어노테이션으로 보호해야 함 [18, 19].
- 평가: Steam 리뷰 요약에서 "외부 콘텐츠 추가가 꽤 어렵다"가 흔한 불만으로 꼽힘 [검색요약].

### 3.6 플러그인 관리자 → 2.2 참고

목록의 체크 ON/OFF, 의존성·순서 경고, 검색, 도움말, Update 갱신은 좋은 평가를 받는다 [25, 31]. 파라미터 복사 단위와 struct 중첩은 불편하다.

### 3.7 테스트 플레이·전투 테스트

- **Playtest** [10, 11]: 메뉴 또는 툴바에서 즉시 실행. Ctrl+이동으로 통행 무시. **F9 디버그 화면**(스위치·변수 실시간 변경). **F8 개발자 도구**(Chromium DevTools). F2 FPS, F3 스트레치, F4 전체화면, **F5 리로드**.
- **Battle Test** [13]: 트룹 화면에서 바로 실행. 탭 [1]~[4]에서 액터·장비·레벨을 지정하고 Status를 미리 계산한 뒤 별도 창에서 전투 시작. 주의: **플러그인 설정은 저장해야 전투 테스트에 반영됨.**
- 애니메이션 미리보기(대상·배경 변경) [14], Sound Test(창을 닫아도 BGM 계속 재생, 작업 BGM으로 활용) [10].

**장점**: "편집 → 1클릭 실행"이 매우 빠르고, **엔티티 단위 테스트(전투 테스트)**가 특히 강력하다.
**한계**: 실행 중 데이터를 수정해도 반영되지 않아 F5 리로드에 의존함(F5 시 data를 다시 읽는지 여부는 **미확인**). 특정 이벤트나 대사 지점부터 시작하는 공식 기능이 없음(**미확인**. 커뮤니티 플러그인으로 해결하는 경우가 많음). 브레이크포인트·변수 감시는 DevTools 수준.

### 3.8 기타 에디터 UX

- 캐릭터 생성기(남/여/아이 파츠 조합 → 얼굴·보행·사이드뷰 배틀러 출력, 설정 저장/불러오기) [10].
- 튜토리얼 내장, 테마(UI Theme), 투명색·그리드 옵션 [10].
- 데이터 포맷: 데이터 JSON은 기본적으로 압축된 형태로 저장되어 Git 병합이 어렵다. 그래서 Pretty JSON 같은 플러그인이 플레이테스트 시점에 재포맷한다. MZ에는 System 2에 "JSON Format Level" 설정이 있다는 댓글이 있으나 공식 도움말 System 2 페이지에는 없음(**미확인**, 후기 버전 추가 가능성). MapInfos.json에 에디터 상태(스크롤 위치)가 섞여 있어 충돌이 잦음. 브랜치 간 맵 ID 충돌 [37, 검색요약].

### 3.9 사용자들이 꼽는 장점·불편 정리

| 구분 | 내용 | 근거 |
|---|---|---|
| 장점 | 비프로그래머도 DB 폼 + 이벤트 리스트로 완성작을 만들 수 있음 | [12, 17], 다수 리뷰 |
| 장점 | 플러그인 커맨드 드롭다운과 파라미터 편집 QoL, 의존성 경고 | [25, 31] |
| 장점 | 이동 경로·픽처·풍선·대사·애니메이션 미리보기 | [5, 28, 36] |
| 장점 | 즉시 테스트 플레이, 전투 테스트, F9 디버그 | [10, 13] |
| 장점 | 해상도·폰트·코어 버전을 에디터에서 관리 | [5, 15, 31] |
| 불편 | "사실상 MV 2". 이벤트 에디터는 거의 동일하고 혁신 부족 | [28, 31] |
| 불편 | MV 플러그인 비호환으로 생태계 재구축 필요, 플러그인 가격 부담 | [2, 28, 31] |
| 불편 | Effekseer 학습 부담, MV 애니메이션 이식 어려움 | [31, 33] |
| 불편 | 외부 리소스 반입이 어려움(규격·폴더 고정) | Steam 리뷰 요약 [검색요약] |
| 불편 | 모달 창 중첩, 파라미터/struct 편집 피로, 개별 파라미터 복사 불가 | [검색요약] |
| 불편 | Git·협업 비친화 데이터(압축 JSON, 에디터 상태 혼재) | [37] |
| 불편 | 모바일 성능·이식 난이도 | [45] |

---

## 4. 모바일 배포 방식과 한계

### 4.1 공식 경로
- **MV**: 배포 대화상자에서 [Android / iOS]를 고르면 `www` 폴더를 출력함. 공식 도움말은 **Crosswalk**(make_apk.py, Ant, SDK r24, API 22)로 APK를 만드는 법을 안내함 [22, 23]. Crosswalk는 이미 개발이 종료된 프로젝트임 **[일반지식·미확인]**.
- **MZ**: 공식 도움말 Output Formats에는 **Windows, macOS, Web browsers**만 있음 [18]. 실제 배포 대화상자에서는 플랫폼이 "**Web browsers / Android / iOS**"로 묶여 표시된다는 외부 문서가 있음 [40]. 즉 **공식 지원은 웹 출력물(index.html + js/data/img/audio/effects/fonts/css)까지**이고, APK/IPA 패키징은 공식 범위 밖이다. MZ용 공식 Android 패키징 가이드는 찾지 못함(**미확인**).
- 옵션: Exclude Unused Files(img/audio/effects), 이미지·사운드 암호화(키 지정). 브라우저 게임 특성상 복제 방지는 보장하지 않는다고 명시 [18].

### 4.2 커뮤니티 패키징 경로 [검색요약]
| 방식 | 내용 | 비고 |
|---|---|---|
| Cordova | Node + Cordova + Android Studio/Gradle로 웹 출력물을 감싸서 APK/AAB 생성 | 가이드 다수(2021년 무렵 자료가 많아 버전이 오래됨) [44, 43] |
| Capacitor | 웹 ZIP → Capacitor 프로젝트 자동 생성(아이콘·터치 컨트롤) → 직접 빌드·서명 | [42] |
| Android Studio WebView 템플릿 | 출력물을 assets에 넣고 빌드. Google Play IAP 포함 옵션과 사이드로딩용 옵션 | HackMD 가이드 [41] |
| 상용 래퍼 | Website 2 APK Builder Pro 등 | 첫 탭 전까지 검은 화면이 나오는 사례 보고 [35] |
| JoiPlay 등 런처 | 플레이어 측 실행기 | 성능 저하(8fps 사례) [45] |

### 4.3 한계
- **서명·AAB·Play 정책 대응·IAP·광고·푸시·업적**은 전부 래퍼 쪽에서 직접 구현해야 함. "IAP는 Capacitor를 따로 설정해야 한다"는 언급 있음 [검색요약].
- **WebView 성능 편차**: 구형 기기 플러그인 호환 문제, 발열, 저사양에서 fps 저하 [45]. MZ는 WebGL 필수 [18].
- **오디오**: MV 시절 Android Chrome에서 암호화 음성이 지연되는 문제가 공식 문서에 명시됨 [22]. MZ의 현재 상태는 **미확인**.
- **입력·UI**: 키보드 기반 UI를 터치로 이식한 수준이며 세이프에어리어, 노치, 백버튼 처리는 **미확인**(공식 기능 없음으로 추정).
- **세이브**: 웹 저장소 기반이라 앱 삭제 시 데이터가 사라지는 등 플랫폼 정책의 영향을 받음 **[일반지식·미확인]**.

**설계 시사점**: 우리 툴은 **Android 빌드 파이프라인을 1급 기능으로** 둬야 한다. 예: Unity/Godot식 Export Preset(패키지명, 버전코드, 키스토어, 아이콘, 방향). 엔진 선택에 따라 네이티브 런타임(Godot/Unity) 또는 WebView 래퍼(Capacitor)를 고르되, IAP·세이브·백버튼·세이프에어리어는 런타임 표준 서비스로 제공한다.

---

## 5. 결론: 우리 툴 에디터 UX 설계 지침

### 5.1 가져올 것 / 개선할 것

| 영역 | RPG Maker에서 가져올 것 | 개선할 것(우리 툴) |
|---|---|---|
| 데이터베이스 | 좌측 카테고리 → ID 리스트 → 상세 폼 3단 레이아웃, 고정 ID 참조, 우클릭 복사/붙여넣기, 검색 | **카테고리를 스키마로 정의**(카드·덱·캐릭터·스킬·상태·보상·챕터 등을 사용자가 추가). 모달 Traits 창 대신 인라인 리스트 편집. 다중 선택 일괄 편집. 정렬·필터·태그. 붙여넣기 시 diff 확인 |
| 노트 메타데이터 | 엔티티별 확장 데이터 슬롯 | 자유 텍스트 노트 대신 **타입 있는 커스텀 필드**(스키마 검증, 자동완성) |
| 이벤트/시나리오 | 위에서 아래로 읽는 **명령 리스트**, 카테고리 탭 명령 팔레트, 공통 이벤트(재사용), 배틀 이벤트(조건 + Span) | 명령 폼을 **우측 인스펙터에 인라인 표시**(모달 제거). 대사는 **스크립트 텍스트 모드**(예: `화자: 대사` 형식)와 리스트 모드를 양방향으로 전환. 검색/치환, 북마크, 접기 |
| 대사 미리보기 | 이름/얼굴/창 위치/폭 가이드/제어문자 삽입 메뉴/Preview 버튼 | **화자 DB 연동**(이름·표정 세트·보이스), 스탠딩 CG·배경·BGM까지 포함한 **"여기서부터 재생"** 미리보기 |
| 플러그인/확장 | 어노테이션 → 자동 GUI, 커맨드 드롭다운, 의존성·순서 경고, ON/OFF | 스키마 하나로 **인스펙터·명령 폼·런타임 타입 파싱**을 모두 생성. 파라미터 개별 복사, 프리셋 |
| 전투 | DB 설정 하나로 전투 규칙 교체, 데미지 식, **전투 테스트(파티 구성·레벨 지정)** | 카드 전투용 **덱 시뮬레이터**(덱·핸드·시드 고정), 식 에디터와 샘플 계산, 로그·리플레이 |
| 연출 | 애니메이션 DB의 프레임별 SE·플래시 트랙과 대상/배경을 바꿔 가며 미리보기, 픽처 이징 | 내장 **타임라인 에디터**(컷인·셰이크·카메라·SE), 외부 툴 의존 최소화 |
| 리소스 | 폴더별 리스트 + 미리보기, 미사용 파일 제외, 공식 리소스 보호 등록(MaterialBase) | **GUID 기반 참조**(이름을 바꿔도 안전), 드래그앤드롭 임포트, 오디오 인라인 재생, 역참조("어디서 쓰이는지") |
| 테스트 | 1클릭 플레이테스트, F9 디버그(스위치/변수), DevTools, 엔티티 단위 테스트 | **핫리로드**(DB·시나리오 수정 즉시 반영), 특정 노드/챕터부터 시작, 상태 스냅샷 저장/로드, 실기기 Android 원클릭 실행 |
| 프로젝트 설정 | 해상도·폰트·게임 ID·코어 버전을 에디터에서 관리 | 모바일 해상도/세이프에어리어 프리뷰, Export Preset(Android) |
| 데이터 포맷 | JSON 데이터(사람이 읽을 수 있음) | **엔티티별 파일 분리 + 안정 정렬 + 줄 단위 포맷**(Godot .tres처럼), 에디터 상태는 별도 파일(.gitignore) |
| 편집 안전성 | OK/Cancel/Apply 트랜잭션, 전각→반각 자동 변환 | **전역 무제한 Undo/Redo**(커맨드 패턴), 자동 저장, 참조 무결성 검사(삭제 시 경고) |

### 5.2 Unity·Godot 방식으로 보완할 지점

1. **인스펙터(Unity) + 리소스 타입(Godot)**: DB 항목 하나를 Godot `Resource`/Unity `ScriptableObject`처럼 **타입 있는 리소스**로 다룬다. 선택하면 우측 인스펙터에 표시하고, 모달은 없앤다. RPG Maker의 3단 DB는 "필터된 리소스 브라우저 + 인스펙터"로 재해석한다.
2. **프리팹 / 씬 상속**: 카드나 적의 **베이스 템플릿 → 변형(Override 표시)**. RPG Maker에는 상속 개념이 없어 복사/붙여넣기로 데이터를 중복시킨다. 변경된 필드만 굵게 표시하고 "Revert to base"를 제공한다.
3. **드래그앤드롭**: 리소스 브라우저 → 인스펙터 슬롯, 리소스 → 시나리오 리스트(이미지를 놓으면 "CG 표시" 명령 생성, 오디오를 놓으면 "BGM 재생" 명령 생성).
4. **시그널(Godot)**: 배틀 이벤트의 조건 + Span을 **이벤트 버스/시그널 구독**으로 일반화한다(`on_turn_end`, `on_hp_below`, `on_card_played`). 에디터에서는 "트리거 → 조건 → 명령 리스트" 형태로 노출한다. RPG Maker의 Moment Span처럼 무한 반복되는 함정은 엔진이 막는다.
5. **노드·씬 트리(Godot)**: 시나리오 장면을 "장면(씬) → 컷 → 명령"의 트리로 묶고, 비주얼노벨 분기는 **그래프 뷰**(노드 = 장면, 엣지 = 선택지/조건)와 리스트 뷰를 오가며 편집한다.
6. **Play 모드(Unity)**: 에디터 안에 게임 뷰를 임베드하고 실행 중 인스펙터에서 값을 바꿔 즉시 확인한다. 종료 시 되돌릴지 유지할지 선택하게 한다. RPG Maker의 F9 디버그는 **변수/플래그 워치 패널**로 확장한다.
7. **텍스트 리소스(Godot .tres/.tscn)**: 모든 데이터를 diff·병합 가능한 텍스트로 저장한다. 1인 개발이어도 Git 롤백과 브랜치 실험이 크게 이득이다(MZ 사용자들이 플러그인으로 해결하는 문제를 처음부터 없앤다).
8. **에디터 확장(Unity CustomEditor / Godot EditorPlugin)**: RPG Maker 어노테이션의 장점(선언 → 자동 UI)을 살리면서, 필요할 때는 **커스텀 인스펙터 코드**로 탈출할 수 있는 2단 구조로 만든다.
9. **Export 파이프라인**: Unity Build Settings / Godot Export Presets처럼 Android 서명·버전·아이콘·방향을 프로젝트 설정에 넣고, 원클릭으로 실기기에 설치·실행한다.

### 5.3 우선순위 제안 (1인 개발 기준)
1. 스키마 기반 DB + 인스펙터(무제한 Undo, 텍스트 저장)
2. 시나리오 에디터(리스트/스크립트 양방향 + 대사 미리보기 + "여기서부터 재생")
3. 전투 테스트 / 덱 시뮬레이터
4. 리소스 브라우저(GUID·드래그앤드롭·역참조)
5. Android Export Preset + 실기기 실행
6. 연출 타임라인, 분기 그래프 뷰(후순위)

---

## 6. 미확인 항목 목록

- MZ 코어의 라이브러리 구성(PIXI v5, Effekseer WASM, 세이브 압축/저장 라이브러리, ogg 전용 여부) 및 "파일 구성 통합"의 정확한 범위
- MZ 플러그인 어노테이션 전체 태그 목록(`@base`/`@orderAfter`/`@orderBefore`, 타입 목록, `@noteParam`류)과 `PluginManager.registerCommand` 시그니처: 공식 작성 문서(tkool.jp, developer.rpgmakerweb.com) 열람 실패
- DB 화면의 Undo 지원 범위, 이벤트 명령 리스트 내 검색/치환 기능 유무
- F5 리로드 시 data를 다시 읽는지, 특정 지점부터 테스트를 시작하는 공식 기능 유무
- "JSON Format Level" 설정의 존재 여부와 도입 버전
- MZ 배포 대화상자의 정확한 플랫폼 항목(공식 도움말에는 Windows/macOS/Web만, 외부 문서에는 "Web browsers / Android / iOS")
- MZ용 공식 Android 패키징 가이드 존재 여부, Android에서의 오디오·백버튼·세이프에어리어 처리
- MZ 1.9.0 이후 에디터 패치노트(공식 원문 열람 실패), TPB 사용자 평가, 정량 성능 벤치마크
- 시리즈 역사(2000~VX Ace) 세부 사항: 일반지식 기반, 1차 출처 재확인 안 함
