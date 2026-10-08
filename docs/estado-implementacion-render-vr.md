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
- `Tools/cmd/compilar-android.bat` terminó correctamente tras los últimos cambios. Este comando compila C++ y genera un APK nativo, pero reutiliza contenido cocinado anterior: el APK resultante **todavía no demuestra** que los nuevos materiales estén disponibles en Quest.
- La creación de los tres `.uasset` terminó en Unreal Editor sin errores. No se instaló el APK nuevo en el visor ni se cambió el release probado.

## Bloqueo para completar y validar en Quest

La revisión automática rechazó la ejecución de `Cook` porque `AGENTS.md` conserva la instrucción previa de no cocinar shaders nuevos y consideró que “implementa todo” no la revocaba expresamente. No se intentó el mismo paso por otra vía. Falta autorización explícita del usuario para cocinar shaders nuevos y empaquetar un APK de prueba; después se debe instalar en Quest, comparar materiales y sombras allí, medir FPS y memoria, y corregir cualquier regresión antes de crear un nuevo release.

El plan completo y los criterios de aceptación están en [plan-mejoras-visuales-quest3s.md](plan-mejoras-visuales-quest3s.md).
