# REA 설치·MCP 통신 검증 — 2026-10-26

## 현재 Codex MCP의 Ghidra 환경 확인 완료

2026-10-26 현재 Codex의 `binary_session({expected_package_version: "6.8.0"})` 직접 호출에서 Ghidra 후보가 `available`로 응답했다. Ghidra 12.1.4 설치 루트와 Java 21.0.8을 인식했고 준비 검사 8개 모두 통과했다. REA 6.8.0 / codex-mcp-client 0.164.0 / alignment `aligned`. 열린 대상과 정리 대기는 없다. [원시 응답](codex-ghidra-session-check-20261026.json)을 저장했다. 환경 확인만 수행했으며 새 대상 분석은 하지 않았다. 아래 재시작 후 확인 대기·Ghidra 미설정 표기는 과거 상태이며 이 결과가 우선한다.

## 이전 Ghidra 준비 완료

Ghidra 12.1.4 PUBLIC + JDK 21.0.8 검사 통과 및 Codex REA 설정 저장 완료. 별도 CLI에서 Windows hostname.exe의 동일 해시 사본을 실행하지 않고 진입 함수 `0x140001300`을 디컴파일했다. Ghidra provider 12.1.4가 실제 의사 코드를 반환했고 종료 코드 0이었다. [원시 Evidence](ghidra-native-result.json), [요청·해시](ghidra-native-request.json), [검사](ghidra-12.1.4-doctor.json), [사용 안내](../../GHIDRA_SETUP.md)를 참고한다. **Codex 재시작 후 실행 중인 MCP가 새 Ghidra 환경을 읽었는지 확인하는 작업은 남아 있다.** 아래 Ghidra 미설정 표기는 이전 REA 연결 검증 당시 상태다.

## 확인 결과

- Node.js 24.22.1 / npm 11.19.1 / Codex CLI 0.164.0 실행 확인.
- 공식 npm `rea-agents@6.8.0`의 `setup --client codex --dry-run --format json` 계획 확인 후 같은 클라이언트 범위로 `--yes` 적용.
- `C:\Users\kjk98\.codex\config.toml`에 REA 등록. 실행 명령은 `cmd.exe /d /c npx -y rea-agents@6.8.0 mcp`, 시작 제한 30초, 도구 제한 450초.
- 기존 설정 백업: `C:\Users\kjk98\.codex\config.toml.rea.backup`. 설치 후 파일이 백업 원문으로 시작함을 검사해 기존 설정 보존 확인. 다른 MCP 설정 내용·비밀값은 검증 기록에 복사하지 않음.
- 대응 스킬: `C:\Users\kjk98\.agents\skills\reverse-engineer-anything` (metadata version 49, tool_count 229). 공식 패키지의 대응 스킬을 함께 설치했으며 별도 저장소 최신 스킬로 교체하지 않음.
- 설치 후 `doctor --client codex --format json`: `healthy: true`, Codex registration과 skill 모두 `aligned`.
- 별도 로컬 stdio 검증 클라이언트에서 `initialize` → `notifications/initialized` → `tools/list` 성공. 서버 `rea` 6.8.0, 실제 도구 229개.
- 실제 MCP `tools/call`로 `analyze_javascript_application`, `export_evidence_bundle` 성공. CLI 분석으로 대체한 결과가 아님.

**후속 Codex 세션에서 통합 연결 확인 완료(2026-10-26).** 실제 도구 목록에 REA 229개가 노출됐고, 현재 Codex의 `mcp__rea__binary_session({expected_package_version: "6.8.0"})` 직접 호출이 성공했다. 응답은 서버 `rea` 6.8.0, 클라이언트 `codex-mcp-client` 0.164.0, 프로토콜 `2025-06-18`, 정렬 상태 `aligned`를 보고했다. 열린 대상과 정리 대기는 없으며 분석을 반복하거나 설정을 변경하지 않았다. [현재 세션 원시 응답](codex-session-check-20261026.json)을 보관했다. Ghidra는 현재 서버에서 `not_configured`이며 네이티브 분석은 이 확인 범위 밖이다. 위 별도 클라이언트 검증과 이번 현재 세션 검증을 구분한다.

## 작은 분석 1건

설치 검증용으로 기존 참고 자료 `refs/rpgmaker-mv-corescript`에서 `js/rpg_objects/Game_Interpreter.js`만 선택했다. 전체 소스 저장소 구조 분석에 REA를 반복 사용할 필요는 없으며, 이번 호출은 사용자가 요청한 설치 후 기능 검증이다.

- JavaScript 1개 파싱, 파싱 실패 0, AST 노드 8,665개 방문.
- 파일 SHA-256: `b2865110da4680a33f700569ee17d74187b0fbd2720f84ff8d12b593f4b438be`.
- 부모 Evidence: `ev_a9ba83bc2c71f9643c5a8bddd94dc0e363995bcd6cd6137eb9e4bccc551b4814`.
- 요약 Evidence: `ev_e2e72edd43bbc8796a88df4474fd5f6ab6b75aee5d9e7503bdf8488b370c12b6`.
- 선택하지 않은 186개 파일과 동적 관계는 미분석/미해결로 남음. 전체 관계 그래프는 partial이며 런타임 동작을 증명하지 않음. Ghidra·네이티브 분석은 검증 범위 밖.

REA가 기록한 정적 표현 노드와 원본을 대조한 결과:

| 원본 위치 | REA 관측 | 원본 대조 해석 |
|---|---|---|
| Game_Interpreter.js:77 | `read:/updateChild`, `read:/updateWait` | 자식 이벤트 또는 대기 처리가 진행 중이면 현재 업데이트 루프를 멈춤 |
| Game_Interpreter.js:83 | `read:/executeCommand` | 다음 명령 실행 결과가 false이면 루프를 멈춤 |
| Game_Interpreter.js:168 | `write:/prototype/executeCommand` | 173~176행에서 명령 코드로 메서드 이름을 조합해 호출함. 동적 대상의 완전한 호출 관계는 정적 그래프만으로 확정하지 않음 |
| Game_Interpreter.js:312,336 | `write:/prototype/command101`, `read:/setWaitMode` | 대화 명령이 텍스트를 등록하고 message 대기로 전환함 |

위 해석은 REA 자동 설명문이 아니라 REA 위치 정보와 원본 코드를 대조한 분석자 해석이다. 기존 `analysis/rpgmaker/01~07` 문서를 REA로 다시 검증한 것은 아니다.

## 원시 기록과 재현

- [검증 코드](../../tools/verify_rea_mcp.py): `python engine-tool/tools/verify_rea_mcp.py --analyze`. 설치된 명령으로 서버를 시작하고, 분석 원본을 실행하지 않고 읽는다. 실행마다 요약/목록 기록을 갱신하고 전체 근거는 시각을 붙인 새 파일로 저장한다.
- `initialize.json`, `tools-list.json`: 실제 초기화 응답과 전체 도구 스키마.
- `analysis-request.json`, `analysis-summary.json`: 성공한 분석 요청과 요약 응답.
- `evidence-20261026-230007.json`: 서버가 내보낸 전체 Evidence 2건, 10,166,248바이트.
- `export-result.json`, `stderr.log`: 내보내기 결과와 성공 호출 로그.
- `analysis-result.json`: 최초 전체 응답의 10MiB 전송 한도 오류. 서버가 분석 완료를 보고했지만 전송에 실패한 기록이다. 후속 호출은 `detail: summary`로 변경하고 전체 근거를 파일로 내보냈다. 응답 한도·Codex 설정은 추가 변경하지 않았다.

샌드박스의 첫 npm 다운로드는 EACCES로 실패했고 승인 실행에서 정상 동작했다. 이후 설치·MCP 검증은 승인된 사용자 환경에서 수행했다. 전역 git safe.directory를 추가하거나 기존 Node·Java·Ghidra 설정을 변경하지 않았다.

공식 출처: [REA 설치 안내](https://github.com/morluto/rea/blob/main/docs/installation.md), [REA 패키지](https://github.com/morluto/rea/blob/main/package.json), [OpenAI MCP 안내](https://learn.chatgpt.com/docs/extend/mcp?surface=cli).
