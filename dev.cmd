@echo off
rem ============================================================================
rem  Naru Studio 개발용: 빌드 -> (테스트) -> 실행
rem
rem    dev            빌드한 뒤 마지막으로 연 프로젝트로 바로 실행 (더블클릭도 이것)
rem    dev test       빌드한 뒤 전체 테스트 (ctest)
rem    dev check      빌드 + 전체 테스트, 모두 통과하면 실행
rem    dev build      빌드만
rem    dev clean      빌드 폴더를 지우고 처음부터 다시 빌드한 뒤 실행
rem
rem  Qt/Python 경로는 NARU_QT_DIR / NARU_PYTHON_DIR 환경변수로 바꿀 수 있다.
rem  지정하지 않으면 C:\Qt\6.*\msvc2022_64 와 "py -3"의 Python을 찾는다.
rem ============================================================================
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=run"
set "PRESET=windows-msvc"
set "BIN=build\%PRESET%\bin"

if /i not "%MODE%"=="run" if /i not "%MODE%"=="test" if /i not "%MODE%"=="check" if /i not "%MODE%"=="build" if /i not "%MODE%"=="clean" (
    echo 사용법: dev [run^|test^|check^|build^|clean]
    exit /b 2
)

rem ---- MSVC 개발 환경 -------------------------------------------------------
where cl >nul 2>nul
if not errorlevel 1 goto :msvc_ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [오류] Visual Studio Build Tools를 찾지 못했습니다. docs\BUILD.md를 보세요.
    goto :fail
)
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo [오류] MSVC x64 C++ 도구가 설치된 Visual Studio가 없습니다.
    goto :fail
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto :fail
:msvc_ready

rem ---- Qt / Python 경로 -----------------------------------------------------
if not defined NARU_QT_DIR (
    for /d %%d in ("C:\Qt\6.*") do if exist "%%d\msvc2022_64\bin\Qt6Core.dll" set "NARU_QT_DIR=%%d\msvc2022_64"
)
if not defined NARU_QT_DIR (
    echo [오류] Qt msvc2022_64 kit을 찾지 못했습니다. NARU_QT_DIR 환경변수를 지정하세요.
    goto :fail
)
if not defined NARU_PYTHON_DIR (
    for /f "usebackq delims=" %%p in (`py -3 -c "import sys; print(sys.base_prefix)" 2^>nul`) do set "NARU_PYTHON_DIR=%%p"
)
if not defined NARU_PYTHON_DIR (
    echo [오류] Python을 찾지 못했습니다. NARU_PYTHON_DIR 환경변수를 지정하세요.
    goto :fail
)
echo Qt     : %NARU_QT_DIR%
echo Python : %NARU_PYTHON_DIR%

rem ---- 빌드 -----------------------------------------------------------------
if /i "%MODE%"=="clean" (
    echo 빌드 폴더를 지웁니다: build\%PRESET%
    if exist "build\%PRESET%" rmdir /s /q "build\%PRESET%"
)
if not exist "build\%PRESET%\build.ninja" (
    echo.
    echo === 설정: cmake --preset %PRESET% ===
    cmake --preset %PRESET%
    if errorlevel 1 goto :fail
)
echo.
echo === 빌드 ===
cmake --build --preset %PRESET%
if errorlevel 1 (
    echo.
    echo [빌드 실패] 위의 error 줄을 확인하세요.
    goto :fail
)

if /i "%MODE%"=="build" goto :done
if /i "%MODE%"=="test" goto :test
if /i "%MODE%"=="check" goto :test
goto :launch

rem ---- 테스트 ---------------------------------------------------------------
:test
echo.
echo === 테스트 ===
ctest --preset %PRESET%
if errorlevel 1 (
    echo.
    echo [테스트 실패] 위의 출력을 확인하세요.
    goto :fail
)
echo [테스트 통과]
if /i "%MODE%"=="test" goto :done

rem ---- 실행 -----------------------------------------------------------------
:launch
echo.
echo === 실행: Naru Studio (마지막 프로젝트) ===
start "" "%BIN%\NaruStudio.exe" --last
goto :done

:done
endlocal
exit /b 0

:fail
echo.
rem 더블클릭으로 실행한 경우 창이 바로 닫히지 않도록 기다린다.
echo %CMDCMDLINE% | find /i "/c" >nul && pause
endlocal
exit /b 1
