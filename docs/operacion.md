# Operación en Windows y Quest

Todos los `.bat` se ejecutan desde cualquier carpeta. Resuelven la raíz del proyecto a partir de su propia ubicación y no guardan rutas personales. Si Unreal 5.8 está en otra carpeta, define `UE_ROOT`. Si ADB está en otra ruta, define `ADB_EXE`; también se admite `ANDROID_HOME`. Cuando haya varios dispositivos ADB, define `ANDROID_SERIAL` antes de ejecutar los comandos.

## Comandos

| Comando | Acción | Requisitos |
| --- | --- | --- |
| `Tools\cmd\verificar-entorno.bat` | Comprueba Unreal, ADB, Python, LFS y release. | Git LFS instalado. |
| `Tools\cmd\validar-modelos.bat [archivo.glb]` | Audita estructura y límites de GLB sin modificarlo. | Python 3. |
| `Tools\cmd\iniciar-hub.bat` | Sirve biblioteca HTTP en el puerto 8765. | Python 3. |
| `Tools\cmd\puente-usb.bat` | Crea el puente `adb reverse`. | Quest autorizada por ADB. |
| `Tools\cmd\instalar-release.bat` | Instala APK probado, copia paquetes y abre la app. | Quest, release completo. |
| `Tools\cmd\instalar-candidato-visual.bat` | Instala el candidato visual, OBB y paquetes sin borrar modelos. | Quest, `git lfs pull`; validación en visor pendiente. |
| `Tools\cmd\abrir-app.bat` | Fuerza cierre y abre la app instalada. | Quest con APK y contenido. |
| `Tools\cmd\capturar-log.bat` | Copia el log Unreal a `deploy/logs/quest_actual.log`. | Quest conectada. |
| `Tools\cmd\compilar-android.bat` | Compila C++ Android con `Build.bat`. | Unreal 5.8 y contenido cocinado local previo para APK funcional. |

Ningún script ejecuta `Cook` o `BuildCookRun`. `quest3s-test-actual` conserva la versión probada y `quest3s-visual-candidate` contiene el nuevo paquete todavía pendiente de prueba en el visor. La compilación de código nativo en un clon limpio no sustituye el contenido cocinado de los paquetes.

## Flujo USB

1. Conecta Quest encendida y autoriza depuración USB dentro del visor.
2. `adb devices` debe mostrar `device`, no `unauthorized`.
3. Ejecuta `Tools\cmd\iniciar-hub.bat` en una terminal que permanezca abierta.
4. Ejecuta `Tools\cmd\instalar-release.bat` una vez en un visor nuevo. Para uso posterior basta `Tools\cmd\puente-usb.bat` y `Tools\cmd\abrir-app.bat`.
5. En la app pulsa **VER MODELOS DE LA LAPTOP**. La URL predeterminada `http://127.0.0.1:8765` funciona con el puente USB.

El panel puede aparecer bajo las aplicaciones de orígenes desconocidos de Quest. Si ADB informa que inició la actividad pero no aparece, comprueba que el visor esté despierto, que no haya un diálogo del sistema encima y vuelve a ejecutar `abrir-app.bat`.

## Flujo Wi-Fi

Inicia el hub y abre `http://IP-LAPTOP:8765` desde otro equipo de la misma red. Escribe esa URL en el panel VR y pulsa **VER MODELOS DE LA LAPTOP**. El firewall de Windows debe permitir el puerto local 8765. El hub no tiene autenticación: úsalo solo en una red de confianza.

## Logs y diagnóstico

`capturar-log.bat` guarda el archivo en `deploy/logs/`, carpeta ignorada por Git porque puede contener rutas y URLs de la sesión. Busca líneas `HouseSandbox:` para importación, descarga y botones. Una importación grande puede tardar decenas de segundos sin actualizar la imagen.

Si falta un modelo en la biblioteca, ejecuta `validar-modelos.bat` y revisa que el GLB esté en `Tools/models/`. Si el panel muestra conexión fallida, confirma `iniciar-hub.bat`, el puente USB o la IP Wi-Fi. Si el APK inicia sin recursos en un visor nuevo, vuelve a ejecutar `instalar-release.bat`: el APK requiere los cinco archivos de `Content/Paks`.

## Referencias de rutas

- Identificador Android: `com.Allnovu.testMeta`.
- Actividad: `com.epicgames.unreal.GameActivity`.
- Biblioteca del visor: `/sdcard/Android/data/com.Allnovu.testMeta/files/Houses` en el dispositivo probado.
- Paquetes del visor: `/sdcard/Android/data/com.Allnovu.testMeta/files/UnrealGame/Quest3S_Test/Quest3S_Test/Content/Paks`.
- Log del visor: `/sdcard/Android/data/com.Allnovu.testMeta/files/UnrealGame/Quest3S_Test/Quest3S_Test/Saved/Logs/Quest3S_Test.log`.

El sistema Android puede cambiar la presentación exacta de las carpetas privadas en otras versiones. Los scripts comprueban los fallos de ADB y no eliminan los GLB del visor.
