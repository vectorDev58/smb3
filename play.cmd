@echo off
cd /d "%~dp0"
py -3 tools\smb3.py play
if errorlevel 1 pause
