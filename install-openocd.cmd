@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-openocd.ps1"
endlocal
