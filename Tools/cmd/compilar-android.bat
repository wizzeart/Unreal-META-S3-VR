@echo off
setlocal
call "%~dp0_comun.bat"
if not exist "%UE_BUILD%" (echo No se encontro Unreal: "%UE_BUILD%" & exit /b 1)
if not exist "%UPROJECT%" (echo No se encontro el proyecto & exit /b 1)
if not exist "%PROYECTO_RAIZ%\Saved\Cooked\Android_ASTC" echo AVISO: no hay contenido cocinado local. El APK resultante puede no ser desplegable; usa instalar-release.bat.
echo Compilando codigo nativo Android. No se ejecuta Cook ni BuildCookRun.
call "%UE_BUILD%" Quest3S_Test Android Development "-Project=%UPROJECT%" -WaitMutex -NoHotReloadFromIDE
exit /b %ERRORLEVEL%
