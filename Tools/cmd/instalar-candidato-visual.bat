@echo off
setlocal
call "%~dp0_comun.bat"
if not exist "%ADB_EXE%" (echo No se encontro ADB & exit /b 1)
"%ADB_EXE%" get-state >nul 2>&1 || (echo No hay una Quest autorizada por ADB & exit /b 1)
set "CANDIDATO=%PROYECTO_RAIZ%\deploy\releases\quest3s-visual-candidate"
set "APK=%CANDIDATO%\Quest3S_Test-arm64.apk"
set "OBB=%CANDIDATO%\main.1.%PAQUETE_APP%.obb"
set "PAKS=%CANDIDATO%\Content\Paks"
if not exist "%APK%" (echo Falta el APK. Ejecuta git lfs pull. & exit /b 1)
if not exist "%OBB%" (echo Falta el OBB. Ejecuta git lfs pull. & exit /b 1)
if not exist "%PAKS%\Quest3S_Test-Android_ASTC.ucas" (echo Faltan los paquetes. Ejecuta git lfs pull. & exit /b 1)
"%ADB_EXE%" install -r "%APK%" || exit /b 1
set "OBB_REMOTO=/sdcard/Android/obb/%PAQUETE_APP%"
"%ADB_EXE%" shell mkdir -p "%OBB_REMOTO%" || exit /b 1
"%ADB_EXE%" push "%OBB%" "%OBB_REMOTO%/main.1.%PAQUETE_APP%.obb" || exit /b 1
set "PAKS_REMOTO=/sdcard/Android/data/%PAQUETE_APP%/files/UnrealGame/Quest3S_Test/Quest3S_Test/Content/Paks"
"%ADB_EXE%" shell mkdir -p "%PAKS_REMOTO%" || exit /b 1
for %%F in ("%PAKS%\*.pak" "%PAKS%\*.ucas" "%PAKS%\*.utoc") do (
    if exist "%%~fF" (
        "%ADB_EXE%" push "%%~fF" "%PAKS_REMOTO%/%%~nxF" || exit /b 1
    )
)
"%ADB_EXE%" reverse tcp:8765 tcp:8765 || exit /b 1
"%ADB_EXE%" shell am force-stop %PAQUETE_APP%
"%ADB_EXE%" shell am start -n %PAQUETE_APP%/%ACTIVIDAD_APP%
exit /b %ERRORLEVEL%
