# Despliegue de la versión probada

`releases/quest3s-test-actual/` contiene una copia del APK que se instaló en Quest y de los cinco archivos `Content/Paks` usados por esa instalación. El APK no contiene estos paquetes: **instalar solo el APK no reproduce la app completa en un visor nuevo**.

Ejecuta `git lfs pull`, `Tools\cmd\verificar-release.bat` y `Tools\cmd\instalar-release.bat` con Quest conectada por ADB. El script instala APK, copia los paquetes, crea el puente USB al hub y abre la actividad. No cocina shaders. `SHA256SUMS.txt` permite verificar que la copia del release está íntegra.

`artifacts/` y `logs/` están reservadas para salidas locales futuras y se ignoran en Git. No guardes aquí credenciales, keystores privados ni datos personales. Los GLB de la laptop están en `Tools/models/`; los modelos que el usuario descargue en el visor quedan en su carpeta `Houses` y no se incluyen en este release.

`releases/quest3s-visual-candidate/` contiene el APK Android ASTC, OBB y cinco paquetes de contenido de la rama visual, incluido el arreglo de acciones OpenXR para X+Y y A+B. Es un **candidato pendiente de comprobar dentro del visor**; no sustituye la versión probada. `SHA256SUMS.txt` registra los siete archivos. Con Quest conectada, `Tools\cmd\instalar-candidato-visual.bat` instala sobre la aplicación existente con `adb install -r`, copia OBB y paquetes, configura el puente USB y abre la app. Conserva los modelos de `Houses`.

La versión actual es una captura binaria de desarrollo, sin firma de distribución comercial. Para publicar una versión firmada, hace falta un proceso de entrega específico y autorización separada. Consulta `docs/operacion.md` para comandos y rutas.
