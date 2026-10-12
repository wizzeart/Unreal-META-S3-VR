# Decisiones y límites vigentes

## Versión de referencia

La versión probada en Quest 3S carga GLB desde el hub o almacenamiento local, recorre el interior con movimiento continuo y muestra colores y texturas de base. La corrección de caras duplicadas evitó que superficies interiores aparecieran negras. La app utiliza los materiales `Widget3DPassThrough_*_OneSided` que ya estaban cocinados.

Se probó aplicar `/Game/Weapons/Rifle/Materials/M_Weapon` a las superficies opacas. En la captura del dispositivo oscureció paredes y techo; una luz de relleno posterior sobreexpuso el suelo. Se retiraron ambos cambios. No los reactives sin comparar capturas y fluidez en Quest.

## Restricción de compilación

La instrucción anterior de no cocinar shaders nuevos fue revocada para esta implementación: el usuario autorizó expresamente Cook, empaquetado Android ASTC e instalación temporal en Quest. Los scripts de operación siguen instalando el release probado; `Build.bat` por sí solo compila C++ Android y no genera el contenido nuevo. El paquete de `deploy/releases/quest3s-test-actual` conserva la versión ya probada.

El primer intento de Cook de esta rama falló porque faltaba Windows SDK para compilar `Quest3S_TestEditor` en Win64; UnrealBuildTool solicitaba al menos 10.0.19041.0. Con autorización del usuario se instaló Windows SDK 10.0.26100 mediante Visual Studio Installer. Después, el módulo de editor compiló y Cook Android ASTC terminó correctamente. No se reanudó la descarga de la ISO detenida anteriormente.

## Próximas mejoras posibles sin alterar la versión de referencia

- Medir duración de importación y uso de memoria en varios GLB.
- Revisar el contenido de los archivos antes de exportar desde 3ds Max: imágenes embebidas, tamaño y geometría.
- Mejorar documentación y herramientas de diagnóstico del hub y de ADB.
- Medir en Quest la ruta de materiales iluminados y las sombras de esta rama antes de promoverla a release.

La prioridad es conservar el aspecto que el usuario aprobó y el movimiento fluido. `docs/colaboracion.md` explica cómo registrar cambios y verificaciones.
