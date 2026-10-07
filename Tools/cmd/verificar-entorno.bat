@echo off
setlocal
call "%~dp0_comun.bat"
set "FALTAS=0"
if not exist "%UPROJECT%" (echo FALTA proyecto Unreal: "%UPROJECT%" & set "FALTAS=1")
if not exist "%UE_BUILD%" (echo FALTA Unreal 5.8: "%UE_BUILD%" & set "FALTAS=1")
if not exist "%ADB_EXE%" (echo FALTA ADB: "%ADB_EXE%" & set "FALTAS=1")
where python >nul 2>&1 || (echo FALTA Python 3 en PATH & set "FALTAS=1")
git lfs version >nul 2>&1 || (echo FALTA Git LFS & set "FALTAS=1")
if not exist "%RELEASE_ACTUAL%\Quest3S_Test-arm64.apk" (echo FALTA APK de release. Ejecuta git lfs pull. & set "FALTAS=1")
if not exist "%RELEASE_ACTUAL%\Content\Paks\Quest3S_Test-Android_ASTC.ucas" (echo FALTAN paquetes de contenido. Ejecuta git lfs pull. & set "FALTAS=1")
if "%FALTAS%"=="0" echo Entorno listo. Este script no compila ni despliega.
exit /b %FALTAS%
