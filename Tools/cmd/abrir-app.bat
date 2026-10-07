@echo off
setlocal
call "%~dp0_comun.bat"
if not exist "%ADB_EXE%" (echo No se encontro ADB & exit /b 1)
"%ADB_EXE%" get-state >nul 2>&1 || (echo No hay una Quest autorizada por ADB & exit /b 1)
"%ADB_EXE%" shell am force-stop %PAQUETE_APP%
"%ADB_EXE%" shell am start -n %PAQUETE_APP%/%ACTIVIDAD_APP%
exit /b %ERRORLEVEL%
