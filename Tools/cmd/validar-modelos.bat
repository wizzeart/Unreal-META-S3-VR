@echo off
setlocal
call "%~dp0_comun.bat"
where python >nul 2>&1 || (echo Python 3 no esta en PATH & exit /b 1)
python "%PROYECTO_RAIZ%\Tools\auditar_glb.py" %*
exit /b %ERRORLEVEL%
