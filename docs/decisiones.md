# Decisiones y límites vigentes

## Versión de referencia

La versión probada en Quest 3S carga GLB desde el hub o almacenamiento local, recorre el interior con movimiento continuo y muestra colores y texturas de base. La corrección de caras duplicadas evitó que superficies interiores aparecieran negras. La app utiliza los materiales `Widget3DPassThrough_*_OneSided` que ya estaban cocinados.

Se probó aplicar `/Game/Weapons/Rifle/Materials/M_Weapon` a las superficies opacas. En la captura del dispositivo oscureció paredes y techo; una luz de relleno posterior sobreexpuso el suelo. Se retiraron ambos cambios. No los reactives sin comparar capturas y fluidez en Quest.

## Restricción de compilación

El usuario pidió expresamente **no cocinar shaders nuevos**. No se incluye automatización para cocinar recursos. El paquete de `deploy/releases/quest3s-test-actual` es una copia de la versión ya probada, no una compilación nueva. `Build.bat` de Unreal puede compilar C++ Android sobre contenido ya cocinado del entorno actual; un clon limpio no genera un APK completo solo con ese comando.

La máquina de desarrollo carece del componente C++ x64 de Windows SDK que Unreal pedía para compilar `Quest3S_TestEditor Win64` en la ruta de empaquetado completo. Hubo un intento de instalación por el instalador oficial; no completó la instalación. Tras la instrucción del usuario se detuvo la descarga de la ISO, se eliminaron los archivos parciales y no se instaló el SDK.

## Próximas mejoras posibles sin alterar la versión de referencia

- Medir duración de importación y uso de memoria en varios GLB.
- Revisar el contenido de los archivos antes de exportar desde 3ds Max: imágenes embebidas, tamaño y geometría.
- Mejorar documentación y herramientas de diagnóstico del hub y de ADB.
- Cuando exista autorización específica para cambiar el renderizado, diseñar y medir una ruta PBR móvil. Ese trabajo requiere pruebas en Quest y queda fuera de la automatización actual.

La prioridad es conservar el aspecto que el usuario aprobó y el movimiento fluido. `docs/colaboracion.md` explica cómo registrar cambios y verificaciones.
