@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0local_current_qa_oneclick.ps1"
if errorlevel 1 pause
