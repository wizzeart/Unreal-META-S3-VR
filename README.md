# Matrix VR Show para Meta Quest 3S

Proyecto Unreal Engine 5.8 para cargar viviendas GLB desde una biblioteca web local o desde el selector de archivos de Quest y recorrerlas en primera persona. La versión de referencia se probó en Quest 3S con ADB. Este repositorio conserva la aplicación, las herramientas del hub, modelos de prueba y un despliegue binario ya cocinado.

## Empezar

1. Instala Git LFS y ejecuta `git lfs pull` después de clonar. Los `.uasset`, `.glb` y binarios de despliegue usan LFS.
2. Ejecuta `Tools\cmd\verificar-entorno.bat` para revisar Unreal, Python y ADB.
3. Ejecuta `Tools\cmd\iniciar-hub.bat` y abre `http://localhost:8765` en la laptop.
4. Conecta Quest por USB y ejecuta `Tools\cmd\puente-usb.bat`.
5. Para instalar Matrix VR Show, ejecuta `Tools\cmd\instalar-matrix-vr-show.bat`. Se copian el APK y sus paquetes de contenido; no se cocina ningún shader. `Tools\cmd\instalar-release.bat` conserva la versión de referencia anterior.
6. En Quest, usa **VER MODELOS DE LA LAPTOP**. También puedes usar **ABRIR ARCHIVO DEL DISPOSITIVO** o elegir un GLB guardado.

El control izquierdo mueve al jugador; la palanca derecha gira y desplaza el menú cuando está abierto. El gatillo derecho pulsa el panel. Una pulsación corta de **X** abre o cierra la biblioteca; mantén **X + Y** durante 0,6 s para abrir las opciones visuales. Mantén **A + B** durante 1 s para liberar el modelo y volver a la biblioteca.

En **MODELOS** puedes crear una nota sobre la superficie a la que miras (hasta 6 m), escribirla con el teclado del panel y volver a editarla pulsando la nota con el gatillo derecho. Las notas se atraviesan. **GUARDAR PARTIDA ACTUAL** guarda el GLB, la posicion del jugador y hasta 100 notas en `Houses/Saves`; la lista **CARGAR** recupera una partida. Conserva el GLB local con el mismo nombre para poder cargarla. **Y** corto enciende o apaga el sol; **X+Y** sigue abriendo visuales. **B** corto o **VUELO FANTASMA** en MODELOS activa el desplazamiento vertical con la palanca derecha y permite atravesar paredes; desactivalo con **B** o desde el mismo boton una vez fuera de la geometria.

Para añadir voz, mira una superficie y pulsa **GRABAR NOTA DE VOZ EN LA SUPERFICIE QUE MIRO**. La primera vez, acepta el permiso de micrófono. Habla hasta 30 segundos y pulsa **DETENER Y GUARDAR AUDIO**. El audio se reproduce desde el punto al acercarte, con volumen según la distancia y dirección binaural, también en altura. El marcador muestra una bocina y un borde luminoso mientras suena. Púlsalo para pausar, reanudar, silenciar, volver a grabar o borrar. El audio y sus estados de pausa y silencio se conservan con la partida; deja los archivos de `Houses/Voice` junto a `Houses/Saves` si copias partidas a otro visor.

## Instalación autónoma en Quest

`Tools\cmd\instalar-matrix-vr-show.bat` instala la versión **Matrix VR Show** con icono y portada propios, notas, partidas y vuelo. Una vez instalada, se abre desde la biblioteca del visor sin iniciar nada en la computadora. La biblioteca de GLB locales funciona sin conexión; el hub de la laptop es opcional para añadir modelos nuevos. El paquete completo y sus hashes están en `deploy/releases/matrix-vr-show/`.

La identidad ciberpunk usa un portal en forma de M, cian y magenta sobre azul noche. Los originales y variantes están en `Branding/`; `Tools/cmd/generar-recursos-matrix.ps1` regenera los tamaños Android. El identificador Android `com.Allnovu.testMeta` y la carpeta `Houses` se conservan para mantener modelos, ajustes y partidas al actualizar.

Si un interior se ve oscuro, abre **Opciones visuales** con **X + Y** y cambia **Luz ambiental** entre media, alta y apagada. **Y** enciende o apaga el sol para ajustar los contrastes de cada GLB. Los ajustes visuales se guardan en el visor.

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

El importador estima la memoria de las imágenes requeridas y reduce su tamaño antes de decodificarlas para evitar que las últimas queden sin color al alcanzar los 128 MiB. El paquete `deploy/releases/matrix-vr-show/` incluye los materiales cocinados, la identidad visual y los nuevos controles. La app se instaló en Quest y se comprobó que el menú ciberpunk y todos sus botones se ven sin obstáculos; la iluminación de los interiores aún requiere revisión visual en el visor. El release de referencia `deploy/releases/quest3s-test-actual/` permanece intacto; `deploy/releases/quest3s-visual-candidate/` conserva el candidato visual anterior. Consulta [el estado de validación](docs/estado-implementacion-render-vr.md) para más detalles.

## Documentación

- [Guía de uso y 3ds Max](README_HOUSE_SANDBOX.md)
- [Arquitectura](docs/arquitectura.md)
- [Operación y solución de problemas](docs/operacion.md)
- [Contrato GLB](docs/formato-glb.md)
- [Decisiones y límites](docs/decisiones.md)
- [Trabajo en equipo](docs/colaboracion.md)
- [Despliegue](deploy/README.md)

El remoto `origin` apunta a [Unreal-META-S3-VR](https://github.com/wizzeart/Unreal-META-S3-VR). Revisa los permisos de los modelos externos antes de publicar un repositorio público.
