@echo off
cd /d %~dp0
call ReGTByVinz\setup_build_regt.bat
exit /b %errorlevel%
