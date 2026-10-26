# Naru Studio 개발 빌드 도우미 (Windows).
# VS 2022 C++ 개발자 환경을 불러온 뒤 CMake preset으로 configure → build → test를 실행한다.
#
# 사용 예:
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1 -QtDir D:\Qt\6.8.3\msvc2022_64 -SkipTests
#
# 경로 우선순위: 매개변수 → NARU_QT_DIR / NARU_PYTHON_DIR 환경변수 → 표준 설치 위치 자동 탐색.

param(
    [string]$Preset = "windows-msvc",
    [string]$QtDir = $env:NARU_QT_DIR,
    [string]$PythonDir = $env:NARU_PYTHON_DIR,
    [switch]$SkipTests,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

function Import-VsDevEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe가 없습니다. VS 2022 Build Tools(C++ 데스크톱 개발)를 설치하세요." }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "MSVC x64 도구가 포함된 Visual Studio를 찾지 못했습니다." }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    Write-Host "MSVC 환경: $vcvars"
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

if ($Preset -notmatch "engine-only") {
    if (-not $QtDir) {
        $QtDir = Get-ChildItem C:\Qt -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^6\.' } | Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Join-Path $_.FullName "msvc2022_64" } | Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $QtDir -or -not (Test-Path $QtDir)) { throw "Qt msvc2022_64 kit 경로를 -QtDir 또는 NARU_QT_DIR로 지정하세요." }
    if (-not $PythonDir) {
        $py = Get-Command py.exe -ErrorAction SilentlyContinue
        if ($py) { $PythonDir = (& py -3 -c "import sys; print(sys.base_prefix)").Trim() }
    }
    if (-not $PythonDir -or -not (Test-Path (Join-Path $PythonDir "include\Python.h"))) {
        throw "Python 개발 헤더가 있는 설치 경로를 -PythonDir 또는 NARU_PYTHON_DIR로 지정하세요."
    }
    $env:NARU_QT_DIR = $QtDir
    $env:NARU_PYTHON_DIR = $PythonDir
    Write-Host "Qt: $QtDir"
    Write-Host "Python: $PythonDir"
}

Import-VsDevEnvironment

Push-Location $root
try {
    if ($Clean) { Remove-Item -Recurse -Force "build\$Preset" -ErrorAction SilentlyContinue }
    cmake --preset $Preset
    if ($LASTEXITCODE) { throw "configure 실패" }
    cmake --build --preset $Preset
    if ($LASTEXITCODE) { throw "build 실패" }
    if (-not $SkipTests) {
        ctest --preset $Preset
        if ($LASTEXITCODE) { throw "테스트 실패" }
    }
    Write-Host "`n완료. 제작툴: build\$Preset\bin\NaruStudio.exe"
} finally {
    Pop-Location
}
