# Casas VR para Quest 3S

Paquete Android ASTC cocinado y empaquetado el 15 de octubre de 2026. Incluye el APK, el OBB y los cinco archivos de `Content/Paks`. `SHA256SUMS.txt` permite comprobar su integridad. Esta revisión reduce la luz solar y amplía el ajuste de luz ambiental para interiores; su comparación visual en Quest todavía está pendiente.

Con Quest conectada y autorizada por ADB, ejecuta `Tools\cmd\instalar-casas-vr.bat` desde la raíz del repositorio. El script instala sobre `com.Allnovu.testMeta` con `adb install -r`, copia el contenido y abre la app; conserva `Houses/` y las partidas existentes.

Después de instalarla, Casas VR se abre directamente desde la biblioteca de aplicaciones del visor. Los GLB ya guardados en Quest se pueden abrir sin laptop ni cable. El hub local solo es necesario para consultar o descargar modelos desde la laptop.

El release anterior de `deploy/releases/quest3s-test-actual/` permanece intacto.
