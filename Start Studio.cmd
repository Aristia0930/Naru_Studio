@echo off
set "STUDIO_ROOT=%~dp0app"
set "GODOT_EXE=%~dp0refs\godot-4.7.2\bin\Godot_v4.7.2-stable_win64.exe"
if not exist "%GODOT_EXE%" (
  echo Godot executable not found. See engine-tool/app/README.md.
  pause
  exit /b 1
)
if not exist "%STUDIO_ROOT%\logs" mkdir "%STUDIO_ROOT%\logs"
start "" "%GODOT_EXE%" --path "%STUDIO_ROOT%" --log-file "%STUDIO_ROOT%\logs\engine.log"
