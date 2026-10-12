# Estado de la implementación visual VR

Rama: `feature/render-vr-menus`. La versión probada en `deploy/releases/quest3s-test-actual/` permanece intacta.

## Preparado en el código

- Tres materiales `Default Lit` para opaco, máscara y mezcla en `Content/HouseMaterials/`, con parámetros de color base, metal/rugosidad, normal, AO y emisión. `GlbRuntime.cpp` lee mapas glTF embebidos, factores, color sRGB frente a datos lineales y tangentes calculadas para UV0. Mantiene el material anterior para el perfil Referencia. Los mapas que usan `texCoord` distinto de UV0 se omiten con aviso en `HouseSandbox:`.
- Menú VR de tamaño más cómodo, fijo en el mundo al abrirse, con cabecera y estado fijos, pestañas, listas desplazables por la palanca derecha, texto más grande y botón para recentrarlo.
- X corto abre/cierra la biblioteca; X+Y sostenidos abren las opciones visuales; A+B sostenidos liberan la casa activa y vuelven a la pantalla de modelos. El gatillo derecho sigue seleccionando y los acordes no activan también X corto.
- Perfiles Referencia, Equilibrado y Detalle, interruptores individuales y ajustes persistentes en `Houses/visual_settings.json`. Las opciones de mapas ausentes se muestran como no disponibles.
- Luz de relleno limitada y luz direccional con sombras experimentales desactivadas por defecto. El movimiento continuo conserva 4,2 m/s y el giro, 150 °/s.

## Verificación realizada

- `Tools/cmd/verificar-entorno.bat`, `python -m py_compile Tools/house_hub.py Tools/make_demo_room.py` y `git diff --check` pasaron.
- Windows SDK 10.0.26100.0 quedó instalado con autorización del usuario. `Quest3S_TestEditor Win64 Development` y el C++ Android compilaron correctamente tras los últimos cambios.
- Cook Android ASTC finalizó con 727 paquetes cocinados y 0 errores. `RunUAT BuildCookRun -skipcook -stage -pak -iostore -package` terminó con código 0. La comprobación de `Quest3S_Test-Android_ASTC.utoc` con `UnrealPak -List` mostró los tres `M_HouseLit*.uasset`, además del archivo de shaders Vulkan ES3.1.
- El candidato está en `deploy/releases/quest3s-visual-candidate/` con APK, OBB, cinco archivos de contenido y hashes SHA-256. APK y OBB se instalaron por ADB con `install -r`, sin desinstalar la app ni borrar `Houses`; también se copiaron los cinco archivos de contenido actualizados. El log de Android confirma que montó el OBB. Los GLB guardados siguen presentes.
- En la Quest conectada, la app llegó al mapa y mostró el menú de biblioteca. El puntero del gatillo registró impactos sobre los botones. Se cargó `an_overgrown_japanese-style_location.glb`: 71 674 triángulos, 836 materiales iluminados asignados, 17 mapas normales y un mapa de emisión; los tres padres de material aparecieron disponibles en el log. La captura confirma que el GLB se renderiza con texturas. Es un escenario exterior y la vista quedó bajo parte de la geometría, por lo que no sirve aún para evaluar una vivienda desde su punto de entrada.
- Después de una reconexión ADB, `adb reverse tcp:8765 tcp:8765` volvió a estar activo. Los primeros intentos de consultar la biblioteca de la laptop fallaron mientras el puente estaba ausente; la consulta tuvo éxito tras instalar el candidato actualizado, como se indica abajo.
- El usuario confirmó que los acordes X+Y y A+B no funcionaban en ese candidato. OpenXR 5.8 construye las acciones desde los `Input Mapping Context` iniciales; el código anterior solo enlazaba teclas directas y X/A no estaban en un contexto activo. Se creó `IMC_HouseButtons` con cuatro acciones booleanas y se enlazaron con `UEnhancedInputComponent`. Se añadieron mensajes `HouseSandbox:` por pulsación y por acorde completado.
- El arreglo compiló en Win64 y Android. El nuevo Cook Android ASTC terminó con 732 paquetes cocinados y 0 errores; UAT terminó con código 0. `UnrealPak -List` confirmó en el contenedor las cuatro `IA_House*.uasset`, `IMC_HouseButtons.uasset` y los tres materiales visuales. El candidato versionado se actualizó con APK, OBB y cinco paquetes, y sus siete hashes SHA-256 coinciden.
- El candidato actualizado se instaló en la Quest conectada y se restauró `adb reverse tcp:8765 tcp:8765`. El log confirmó varias activaciones de A+B, el retorno a la biblioteca y la respuesta del hub con 8 modelos. Desde esa biblioteca se cargaron `girls_frontline_2_exilium_dormitory.glb`, `loft_16_interior (1).glb` y `an_overgrown_japanese-style_location.glb`. El usuario confirmó directamente en el visor que X+Y abre «Opciones visuales». La captura ADB posterior muestra la escena, pero no el panel; tampoco quedó una línea de X+Y en el tramo del log consultado, así que esa comprobación depende de la confirmación visual del usuario.

## Validación pendiente en el visor

Queda comprobar los cambios de perfil y los interruptores en una vivienda GLB desde un punto de entrada adecuado, la legibilidad y el desplazamiento del panel en el visor, y el coste de sombras/FPS. La captura de la escena exterior quedó muy oscura desde la posición actual y no permite evaluar su calidad visual. El candidato no reemplaza todavía `deploy/releases/quest3s-test-actual/`.

El primer Cook de esta rama necesitó instalar Windows SDK y compilar el módulo de editor. Otro intento de paquete falló porque Cook y UAT apuntaban a almacenes Zen distintos; el candidato final se cocinó y empaquetó usando la misma ubicación predeterminada. No vuelvas a usar `-LocalDataCachePath` solo en Cook si luego UAT empaquetará con Zen predeterminado.

El plan completo y los criterios de aceptación están en [plan-mejoras-visuales-quest3s.md](plan-mejoras-visuales-quest3s.md).
