# Casas VR para Meta Quest 3S

Proyecto Unreal Engine 5.8 para cargar viviendas GLB desde una biblioteca web local o desde el selector de archivos de Quest y recorrerlas en primera persona. La versión de referencia se probó en Quest 3S con ADB. Este repositorio conserva la aplicación, las herramientas del hub, modelos de prueba y un despliegue binario ya cocinado.

## Empezar

1. Instala Git LFS y ejecuta `git lfs pull` después de clonar. Los `.uasset`, `.glb` y binarios de despliegue usan LFS.
2. Ejecuta `Tools\cmd\verificar-entorno.bat` para revisar Unreal, Python y ADB.
3. Ejecuta `Tools\cmd\iniciar-hub.bat` y abre `http://localhost:8765` en la laptop.
4. Conecta Quest por USB y ejecuta `Tools\cmd\puente-usb.bat`.
5. Para instalar la versión probada, ejecuta `Tools\cmd\instalar-release.bat`. Se copian el APK y sus paquetes de contenido; no se cocina ningún shader.
6. En Quest, usa **VER MODELOS DE LA LAPTOP**. También puedes usar **ABRIR ARCHIVO DEL DISPOSITIVO** o elegir un GLB guardado.

El control izquierdo mueve al jugador; la palanca derecha gira. El gatillo derecho pulsa el panel y **X** del control izquierdo abre o cierra la biblioteca.

## Mapa del repositorio

| Ruta | Responsabilidad |
| --- | --- |
| `Source/Quest3S_Test/HouseSandbox.*` | Menú VR, controles, descarga, selector Android y colocación del jugador. |
| `Source/Quest3S_Test/GlbRuntime.*` | Lectura GLB y creación de mallas y texturas en ejecución. |
| `Source/Quest3S_Test/HouseSandbox_UPL.xml` | Puente Android para seleccionar un archivo local. |
| `Config/`, `Content/`, `Build/Android/` | Configuración y recursos Unreal necesarios para la app. |
| `Tools/house_hub.py` | Servidor HTTP local para subir y servir GLB. |
| `Tools/models/` | Biblioteca de GLB de la laptop; incluye los ejemplos existentes. |
| `Tools/cmd/` | Comandos Windows para el equipo. |
| `deploy/releases/` | Copia verificable del APK probado y sus paquetes de contenido. |
| `docs/` | Arquitectura, operación, formato, decisiones y trabajo en equipo. |

## Estado visual y de compilación

Los GLB usan color base, tintes y transparencia básica mediante materiales ya existentes en Unreal. El proyecto selecciona MSAA móvil en su configuración. La importación **no interpreta** mapas normales, metálico, rugosidad u oclusión ambiental como materiales PBR completos. No hay sombras nuevas para la casa importada. Una prueba con otro material se retiró porque oscurecía paredes y sobreexponía el suelo.

La instrucción vigente para este trabajo es **no cocinar shaders nuevos**. Los scripts de este repositorio no ejecutan `RunUAT BuildCookRun` ni el comando `Cook`. El build nativo Android puede reutilizar contenido ya cocinado del equipo actual; para desplegar desde un clon limpio usa `deploy/releases`.

## Documentación

- [Guía de uso y 3ds Max](README_HOUSE_SANDBOX.md)
- [Arquitectura](docs/arquitectura.md)
- [Operación y solución de problemas](docs/operacion.md)
- [Contrato GLB](docs/formato-glb.md)
- [Decisiones y límites](docs/decisiones.md)
- [Trabajo en equipo](docs/colaboracion.md)
- [Despliegue](deploy/README.md)

No hay remoto Git configurado. Los commits son locales hasta que el propietario cree y conecte el nuevo repositorio remoto. Revisa los permisos de los modelos externos antes de publicar un repositorio público.
