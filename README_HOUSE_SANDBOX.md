# Casas VR para Quest 3S

## Del navegador a las Quest

1. Exporta la casa desde 3ds Max a **GLB (glTF 2.0 binario)**. Usa metros como unidad, convierte los objetos a mallas estáticas y evita compresión Draco y animaciones. Un metro del GLB se muestra como un metro en VR.
2. En la laptop, inicia `python Tools/house_hub.py` desde la carpeta del proyecto y abre `http://localhost:8765` en el navegador. Selecciona el archivo `.glb` y súbelo. Queda guardado en `Tools/models`.
3. Abre la aplicación en las Quest. En el panel VR, pulsa **VER MODELOS DE LA LAPTOP** y después el nombre de tu casa. La app descarga, guarda e importa el modelo. Apunta con el control derecho y pulsa su gatillo.

La dirección inicial del hub en el visor es `http://127.0.0.1:8765`, que funciona mientras las Quest estén conectadas por USB y hayas ejecutado `adb reverse tcp:8765 tcp:8765` en la laptop. Si ambos equipos se comunican por Wi-Fi, escribe `http://IP-DE-LA-LAPTOP:8765` en el campo del panel y pulsa **VER MODELOS DE LA LAPTOP**. El servidor usa el puerto 8765.

También puedes pegar una URL directa a un `.glb` o pulsar **ABRIR ARCHIVO DEL DISPOSITIVO** para usar el selector de Android. Los modelos descargados o elegidos quedan en la carpeta privada `Houses` de la app. Pulsa **ACTUALIZAR ARCHIVOS LOCALES** para verlos y abrirlos sin conexión.

Al cargar una casa, el panel se oculta. Pulsa **X en el control izquierdo** para abrirlo o cerrarlo. La palanca izquierda camina en dirección de la mirada a 4,2 m/s; la derecha gira a 150 grados/s. La malla importada genera colisiones para las paredes. Con el panel abierto, la palanca derecha desplaza la lista.

## Formato admitido

El importador carga geometría estática GLB sin compresión, con nodos, transformaciones, posiciones, normales, UV e índices. Centra la caja del modelo en el origen y detecta la superficie transitable dominante para colocar al jugador sobre el suelo. Lee las imágenes PNG y JPEG incrustadas como color base, respeta las superficies de doble cara y añade una luz de recorrido dentro de la casa. El límite es 100 MB por GLB, 350 000 triángulos renderizados y 128 MB de texturas descomprimidas. El archivo `.max` nativo no se abre en las Quest: expórtalo a GLB primero.

**Limitación visual actual:** se muestran las imágenes de color base del GLB, pero aún no se reproducen fielmente los materiales PBR: faltan los mapas de normales, metálico y rugosidad, además de transparencia. Tampoco se admiten Draco ni animaciones.

El hub es un servidor local sencillo sin cuenta ni nube pública. Úsalo en una red de confianza; Android permite HTTP local para esta app.

## Compilación

El código Android se compila con `Build.bat Quest3S_Test Android Development -Project="RUTA/Quest3S_Test.uproject"`. Para cocinar y empaquetar todos los recursos con `RunUAT BuildCookRun` hace falta instalar Windows SDK 10.0.19041.0 o posterior: Unreal compila también `Quest3S_TestEditor Win64`. Un APK hecho solo con `Build.bat` requiere el contenido Android cocinado y desplegado que ya existe en el visor de pruebas.

