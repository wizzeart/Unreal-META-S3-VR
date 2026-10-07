@echo off
setlocal
call "%~dp0_comun.bat"
if not exist "%ADB_EXE%" (echo No se encontro ADB: "%ADB_EXE%" & exit /b 1)
"%ADB_EXE%" get-state >nul 2>&1 || (echo No hay una Quest autorizada por ADB & exit /b 1)
"%ADB_EXE%" reverse tcp:8765 tcp:8765
exit /b %ERRORLEVEL%
