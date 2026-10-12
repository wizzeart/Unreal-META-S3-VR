# Casas VR para Meta Quest 3S

Proyecto Unreal Engine 5.8 para cargar viviendas GLB desde una biblioteca web local o desde el selector de archivos de Quest y recorrerlas en primera persona. La versión de referencia se probó en Quest 3S con ADB. Este repositorio conserva la aplicación, las herramientas del hub, modelos de prueba y un despliegue binario ya cocinado.

## Empezar

1. Instala Git LFS y ejecuta `git lfs pull` después de clonar. Los `.uasset`, `.glb` y binarios de despliegue usan LFS.
2. Ejecuta `Tools\cmd\verificar-entorno.bat` para revisar Unreal, Python y ADB.
3. Ejecuta `Tools\cmd\iniciar-hub.bat` y abre `http://localhost:8765` en la laptop.
4. Conecta Quest por USB y ejecuta `Tools\cmd\puente-usb.bat`.
5. Para instalar la versión probada, ejecuta `Tools\cmd\instalar-release.bat`. Se copian el APK y sus paquetes de contenido; no se cocina ningún shader.
6. En Quest, usa **VER MODELOS DE LA LAPTOP**. También puedes usar **ABRIR ARCHIVO DEL DISPOSITIVO** o elegir un GLB guardado.

El control izquierdo mueve al jugador; la palanca derecha gira y desplaza el menú cuando está abierto. El gatillo derecho pulsa el panel. Una pulsación corta de **X** abre o cierra la biblioteca; mantén **X + Y** durante 0,6 s para abrir las opciones visuales. Mantén **A + B** durante 1 s para liberar el modelo y volver a la biblioteca.

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

La rama `feature/render-vr-menus` incorpora materiales iluminados para color base, metal/rugosidad, normales, oclusión y emisión de GLB, junto con perfiles y controles individuales persistentes. La luz de relleno es opcional; las sombras direccionales son experimentales y están apagadas por defecto. El perfil Referencia conserva el aspecto anterior. Los mapas que requieran otra coordenada de textura distinta de UV0 todavía se omiten.

Los materiales nuevos ya se cocinaron y empaquetaron en `deploy/releases/quest3s-visual-candidate/`, y ese candidato se instaló por ADB. A+B devolvió a la biblioteca en el visor; X+Y abrió las opciones visuales y el usuario pudo desplazarse y cambiar de perfil. Tras detectar interiores oscuros, el candidato recibió ajustes de texturas, tangentes, posición de entrada e iluminación; falta validar visualmente el último APK y medir su rendimiento. El release probado en `deploy/releases/quest3s-test-actual/` sigue siendo la versión anterior; `Tools\cmd\instalar-release.bat` lo instala y `Tools\cmd\instalar-candidato-visual.bat` instala el candidato. Consulta [el estado de validación](docs/estado-implementacion-render-vr.md) antes de usarlo.

## Documentación

- [Guía de uso y 3ds Max](README_HOUSE_SANDBOX.md)
- [Arquitectura](docs/arquitectura.md)
- [Operación y solución de problemas](docs/operacion.md)
- [Contrato GLB](docs/formato-glb.md)
- [Decisiones y límites](docs/decisiones.md)
- [Trabajo en equipo](docs/colaboracion.md)
- [Despliegue](deploy/README.md)

El remoto `origin` apunta a [Unreal-META-S3-VR](https://github.com/wizzeart/Unreal-META-S3-VR). Revisa los permisos de los modelos externos antes de publicar un repositorio público.
