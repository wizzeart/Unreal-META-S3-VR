# Guía para agentes y colaboradores

Lee `README.md` y `docs/arquitectura.md` antes de editar. Este archivo describe el estado real del proyecto; no reemplaza las instrucciones del usuario.

## Reglas de trabajo

- Conserva `Content/`, `Config/`, `Build/Android/` y los GLB existentes. No muevas ni borres recursos sin una petición concreta.
- Para la rama `feature/render-vr-menus`, el usuario autorizó expresamente cocinar shaders nuevos, empaquetar Android ASTC e instalar un APK temporal de prueba en Quest. Conserva intacto `deploy/releases/quest3s-test-actual/`. `Tools/cmd/compilar-android.bat` solo compila código nativo y requiere contenido cocinado previo para un APK funcional.
- `GlbRuntime.cpp` prepara materiales iluminados y mapas glTF para la rama visual. El candidato ya tiene Cook Android ASTC, pero requiere validación visual en Quest. No prometas PBR, AO o sombras dinámicas en el release probado. Comprueba el código, el contenido cocinado y el visor antes de afirmar que la nueva ruta funciona.
- Mantén la proporción glTF 1 metro = Unreal 100 cm. Revisa orientación, suelo, colisión y doble cara si cambias el importador.
- Conserva el movimiento continuo de 4,2 m/s y giro de 150 grados/s salvo nueva instrucción del usuario.
- Usa `rg` para localizar código. Evita añadir rutas personales, números de serie ADB o claves a los scripts.
- Deja los artefactos temporales en carpetas ignoradas. Los modelos y binarios versionados usan Git LFS.
- Antes de un commit: `git status`, `git diff --check`, verificar las rutas staged y describir el cambio en español.

## Puntos de entrada

- Menú, red, input y posición: `Source/Quest3S_Test/HouseSandbox.cpp`.
- Decodificación GLB: `Source/Quest3S_Test/GlbRuntime.cpp`.
- Selector Android: `Source/Quest3S_Test/HouseSandbox_UPL.xml`.
- Servidor web: `Tools/house_hub.py`.
- Scripts operativos: `Tools/cmd/`.
- APK probado y paquetes: `deploy/releases/quest3s-test-actual/`.

## Verificación mínima

Ejecuta `Tools/cmd/verificar-entorno.bat`, `python -m py_compile Tools/house_hub.py Tools/make_demo_room.py`, y comprueba el despliegue solo cuando Quest esté conectado. Para cambios de C++, compila Android y revisa el log `HouseSandbox:`. El contenido de materiales nuevos exige Cook por separado, autorizado para esta rama. Consulta `docs/operacion.md` para comandos concretos.
