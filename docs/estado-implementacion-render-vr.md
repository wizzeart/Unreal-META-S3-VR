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

## Validación pendiente en el visor

El visor muestra un aviso de sistema para pulsar el botón físico de encendido y activar cámaras y micrófonos; ese aviso cubre la app en la captura ADB. Falta verificar a simple vista el menú, los acordes X+Y y A+B, el cambio de perfiles en un GLB con mapas y el coste de sombras/FPS dentro de Quest. Hasta entonces, el candidato no reemplaza `deploy/releases/quest3s-test-actual/`.

El primer Cook de esta rama necesitó instalar Windows SDK y compilar el módulo de editor. Otro intento de paquete falló porque Cook y UAT apuntaban a almacenes Zen distintos; el candidato final se cocinó y empaquetó usando la misma ubicación predeterminada. No vuelvas a usar `-LocalDataCachePath` solo en Cook si luego UAT empaquetará con Zen predeterminado.

El plan completo y los criterios de aceptación están en [plan-mejoras-visuales-quest3s.md](plan-mejoras-visuales-quest3s.md).
