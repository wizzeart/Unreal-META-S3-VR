@echo off
setlocal
call "%~dp0_comun.bat"
where python >nul 2>&1 || (echo Python 3 no esta en PATH & exit /b 1)
echo Hub local en http://localhost:8765
python "%PROYECTO_RAIZ%\Tools\house_hub.py" %*
exit /b %ERRORLEVEL%
