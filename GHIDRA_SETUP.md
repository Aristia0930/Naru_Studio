# Ghidra 설정 및 사용

2026-10-26: Ghidra 12.1.4 PUBLIC + JDK 21.0.8 + REA 6.8.0 조합으로 준비 검사와 실제 CLI 디컴파일을 완료했다.

## 설치와 연결

- 설치 루트: `engine-tool/ghidra_12.1.4_PUBLIC_20260921/ghidra_12.1.4_PUBLIC`.
- JDK: `C:\Program Files\Java\jdk-21`.
- Codex의 REA MCP 환경에 `GHIDRA_INSTALL_DIR`, `JAVA_HOME`을 저장했다. 시스템 전역 Java 설정은 변경하지 않았다.
- 현재 Codex MCP의 `binary_session` 직접 호출로 새 환경 반영 확인 완료(2026-10-26): Ghidra `available`, 버전 12.1.4, Java 21.0.8, 검사 8개 모두 통과. 열린 대상·정리 대기 없음. 이후 설정을 변경했다면 Codex를 완전히 종료·재실행한 뒤 다시 확인한다.
- REA 등록이 이 설치 폴더를 가리키므로 폴더를 이동하거나 삭제하면 경로를 다시 설정해야 한다. setup이 저장한 REA 실행 경로도 npm 캐시 아래이므로 해당 캐시 제거 시 등록 복구가 필요하다.

## Codex에 분석 요청하기

분석할 로컬 EXE·DLL의 경로와 알고 싶은 기능을 지정한다. 예: “이 실행 파일에서 파일 저장 기능을 Ghidra로 분석하고, 관측한 근거와 추론을 구분해 줘.”

REA는 Ghidra의 headless 실행기를 사용하므로 Ghidra 화면을 미리 열어 둘 필요가 없다. 정적 분석은 대상 프로그램 자체를 실행하지 않는다. 분석 근거와 원본 코드의 복원 가능 범위를 구분한다.

## 직접 화면으로 보기

1. 설치 루트의 `ghidraRun.bat`를 실행한다.
2. JDK 경로를 물으면 `C:\Program Files\Java\jdk-21`을 지정한다.
3. `File → New Project → Non-Shared Project`에서 프로젝트를 만든다.
4. `File → Import File`로 EXE·DLL을 가져온다.
5. 가져온 파일을 더블클릭하고 자동 분석 질문에 `Yes`를 선택한다.
6. 함수 목록에서 함수를 선택해 Listing과 Decompiler를 확인한다. 디컴파일 결과는 원래 소스 코드와 같다고 보장되지 않는다.

GUI 실행 여부는 이번에 검증하지 않았다. 실제 검증은 REA CLI의 Ghidra provider로 수행했다.

## 검증 기록

- `analysis/rea-connection/codex-ghidra-session-check-20261026.json`: 현재 Codex MCP의 Ghidra 환경 반영 확인 원시 응답. 대상 분석은 시작하지 않았다.
- `analysis/rea-connection/ghidra-12.1.4-doctor.json`: 준비 검사.
- `analysis/rea-connection/ghidra-setup-plan.json`, `ghidra-setup-result.json`: 설정 계획과 적용 결과.
- `analysis/rea-connection/ghidra-native-request.json`, `ghidra-native-result.json`: 테스트 대상 식별 정보와 실제 디컴파일 Evidence.
- `tools/verify_ghidra.py`: Windows hostname.exe 사본의 진입 함수를 정적 분석한 재현 코드. 테스트 사본은 `analysis/rea-connection/native-verification/`에 보관하며 게임 에셋이 아니다.
