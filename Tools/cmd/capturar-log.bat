@echo off
setlocal
call "%~dp0_comun.bat"
if not exist "%ADB_EXE%" (echo No se encontro ADB & exit /b 1)
"%ADB_EXE%" get-state >nul 2>&1 || (echo No hay una Quest autorizada por ADB & exit /b 1)
set "DESTINO=%PROYECTO_RAIZ%\deploy\logs"
if not exist "%DESTINO%" mkdir "%DESTINO%" || exit /b 1
"%ADB_EXE%" pull "/sdcard/Android/data/%PAQUETE_APP%/files/UnrealGame/Quest3S_Test/Quest3S_Test/Saved/Logs/Quest3S_Test.log" "%DESTINO%\quest_actual.log"
if errorlevel 1 exit /b 1
echo Log guardado en "%DESTINO%\quest_actual.log"
exit /b 0
