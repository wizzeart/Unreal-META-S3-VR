@echo off
for %%I in ("%~dp0..\..") do set "PROYECTO_RAIZ=%%~fI"
set "UPROJECT=%PROYECTO_RAIZ%\Quest3S_Test.uproject"
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "UE_BUILD=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
if not defined ADB_EXE (
    if defined ANDROID_HOME (
        set "ADB_EXE=%ANDROID_HOME%\platform-tools\adb.exe"
    ) else (
        set "ADB_EXE=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe"
    )
)
set "APK_ACTUAL=%PROYECTO_RAIZ%\Binaries\Android\Quest3S_Test-arm64.apk"
set "RELEASE_ACTUAL=%PROYECTO_RAIZ%\deploy\releases\quest3s-test-actual"
set "PAQUETE_APP=com.Allnovu.testMeta"
set "ACTIVIDAD_APP=com.epicgames.unreal.GameActivity"
exit /b 0
