@echo off
cd /d "%~dp0"
py -3 diagnose.py --seconds 60
pause
