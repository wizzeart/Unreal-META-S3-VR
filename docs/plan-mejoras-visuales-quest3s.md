# Plan de render y menús para Quest 3S

## Objetivo y punto de partida

Hacer que los GLB se vean más naturales en Quest 3S sin perder una navegación cómoda. La versión actual usa materiales de interfaz con color base, `baseColorFactor` y transparencia básica; no reproduce los mapas PBR ni las luces del GLB. El proyecto usa mobile forward, MSAA, multiview y `MobileHDR=False`. El panel actual es un `UWidgetComponent` de 700 × 850, unido a la cámara, con una sola columna desplazable. X lo abre o cierra; el gatillo derecho pulsa y la palanca derecha desplaza la lista.

El trabajo se hará en una rama nueva. Se conservarán los GLB, la escala de 1 m glTF = 100 cm Unreal, el suelo, la colisión y el movimiento de 4,2 m/s con giro de 150 °/s. El release probado seguirá disponible como referencia. Esta etapa es solo planificación: no ejecuta `Cook` ni sustituye el APK. Los materiales nuevos tendrán que cocinarse y empaquetarse para Android antes de poder probarse en Quest; `compilar-android.bat` por sí solo no los incorpora.

## Controles y estados

Estados excluyentes: `Exploración`, `Biblioteca` y `Opciones visuales`. Al abrir cualquiera de los paneles se detiene el movimiento y el giro; al cerrarlo se restablecen. Se mantiene el puntero del controlador derecho y su gatillo.

| Entrada | Resultado | Prevención de conflictos |
| --- | --- | --- |
| X, pulsación corta | Abrir/cerrar la biblioteca actual | Ejecutar la acción al soltar X y tras una ventana breve de detección de acorde; no dispararla si se pulsó Y. |
| X + Y, mantener 0,6 s | Abrir/cerrar `Opciones visuales` delante del usuario | Registrar ambas pulsaciones y consumir el acorde una sola vez; dar respuesta visual al completar el tiempo. |
| A + B, mantener 1 s | Volver a la primera pantalla de la biblioteca | Cancelar acciones pendientes, liberar la casa activa, recentrar al jugador y consultar de nuevo el hub. No reiniciar la app. |
| Gatillo derecho | Activar el control señalado | Liberar cualquier pulsación de puntero al cambiar de panel. |
| Palanca derecha vertical | Desplazar la lista enfocada | Desplazamiento con zona muerta, velocidad proporcional y límites visibles. |
| Botones en pantalla | `Volver a modelos`, `Cerrar`, `Recentrar panel` | Ofrecer la misma navegación sin memorizar acordes. |

Antes de fijar A+B hay que auditar los `InputMappingContext` del template y verificar las teclas en el visor: `IMC_Default` se retira en C++, pero otros contextos permanecen cargados. Las pulsaciones breves de X deben seguir funcionando aunque el usuario intente el acorde X+Y y lo suelte incompleto. Si una descarga termina después de `Volver a modelos`, un identificador de generación debe impedir que abra una casa antigua.

## Nuevo diseño de la biblioteca

- Panel de mundo colocado frente a la vista al abrirse, a unos 1,2–1,5 m, estable en el espacio y recentrable. Probar una superficie física cercana a 0,85 × 0,60 m con `DrawSize` de alta resolución; ajustar después en el visor, no solo en el editor.
- Cabecera fija con pestañas `Modelos de laptop`, `En el visor` y `Opciones visuales`; estado del hub y nombre de la casa cargada. Pie fijo con controles disponibles y estado/error legible. El centro es la única zona que se desplaza.
- Tarjetas o filas grandes con nombre completo en varias líneas, origen, tamaño cuando se conozca y acciones claras: `Cargar` o `Descargar y abrir`. Separar URL avanzada y selector Android en una sección secundaria. Mostrar carga, descarga, error y lista vacía como estados distintos.
- Conservar pestaña, posición de scroll y foco al refrescar; evitar que `Populate` reconstruya toda la interfaz ante cada mensaje. Permitir desplazamiento con palanca aun cuando el rayo no esté sobre la barra; mostrar indicador de página o posición.
- Área de toque de aproximadamente 4–5 cm para los botones principales, contraste alto, texto grande, y pruebas de lectura con el visor puesto. Mantener la biblioteca utilizable con docenas de modelos mediante paginación o lista virtualizada.

## Render: orden de implementación

| Etapa | Cambio | Condición para aceptarlo |
| --- | --- | --- |
| 0. Referencia | Capturas y métricas de 3 GLB: sala simple, interior texturizado y escena más pesada. Medir carga, memoria, FPS, tiempo de GPU y estabilidad térmica en Quest. | Resultados reproducibles y release actual intacto. |
| 1. Materiales móviles | Crear padres cocinados para opaco, máscara y mezcla, con ruta `Default Lit` móvil y ruta de color base actual como respaldo. Elegir instancias en `GlbRuntime.cpp`, nunca compilar shaders al abrir un GLB. | Color base y alfa iguales o mejores que la referencia; sin paredes negras. |
| 2. Datos glTF | Leer factores metálico/rugosidad, textura de rugosidad en G y metal en B, normal, oclusión en R, emisivo y `doubleSided`. Tratar color base/emisivo como sRGB y mapas de datos como lineales. Leer `TANGENT` o generarla correctamente; respetar `texCoord` y comunicar mapas omitidos. | Materiales de prueba conocidos se parecen a un visor glTF de referencia; normal no aparece invertida. |
| 3. Texturas y geometría | Compartir imágenes repetidas, crear mipmaps para reducir parpadeo, poner límites medidos a resolución y memoria, y descargar recursos al cambiar de casa. Revisar la duplicación de caras: usar `Two Sided` solo donde haga falta y comprobar colisión por separado. | Sin aumento inaceptable de memoria o tirones, interiores visibles por ambos lados cuando corresponda. |
| 4. Iluminación | Luz ambiental suave y controlada, una luz direccional para escenas donde tenga sentido, luces locales sin sombras solo si el GLB las define o el usuario las agrega. Retirar la dependencia visual del foco unido a la cabeza. Exposición y balance de color fijos por perfil, comparados dentro del visor. | Paredes legibles sin techo negro ni suelo sobreexpuesto; aspecto consistente al moverse. |
| 5. Sombras | Prototipo aislado de sombras direccionales móviles con distancia y cascadas limitadas. Medir coste y defectos en interiores. Para objetos sin sombra viable usar AO del GLB o textura horneada, sin simular que es una sombra dinámica. | Solo se ofrece el interruptor `Sombras` si realmente afecta al GLB y cumple el presupuesto en Quest. |
| 6. Menús y perfiles | Implementar estados y acordes; conectar interruptores a instancias de material, luces y recursos disponibles; guardar ajustes validados. | Cada interruptor produce un cambio visible, se revierte y no rompe la carga de modelos. |
| 7. Paquete de prueba | Cocinar únicamente los nuevos materiales/permutaciones Android necesarias en un flujo separado, instalar una compilación de prueba y comparar con el release existente. Crear un release nuevo y manifiesto de hashes solo tras validar. | APK funcional en Quest, contenido íntegro y ruta clara de retorno a la versión probada. |

La AO de pantalla, SSR, bloom y otros efectos de posproceso no son parte de la primera entrega: la configuración actual es mobile forward con HDR desactivado. UE 5.8 ofrece más efectos en otras rutas móviles, pero cambiar `MobileHDR`, el renderizador o las permutaciones de shaders requiere un paquete distinto y una prueba específica de multiview, nitidez, coste y compatibilidad. Un control de menú no puede habilitar por sí solo una función que no se cocinó o que exige reiniciar el renderizador.

Archivos previstos: `GlbRuntime.cpp/.h` para mapas, tangentes y selección de materiales; nuevos materiales bajo `Content/` para las variantes Android; `HouseSandbox.cpp/.h` para luces, estados, atajos y ciclo de vida de la casa; un componente o estructura de ajustes para persistencia; `Config/DefaultEngine.ini` solo si las pruebas justifican un cambio global. La presencia de opciones de Lumen, ray tracing o sombras virtuales en el INI actual no demuestra que funcionen en el renderizador Quest.

## Opciones visuales propuestas

En la parte superior: perfiles `Referencia`, `Equilibrado` y `Detalle`. Debajo, interruptores individuales con estado, breve explicación y coste estimado medido. `Referencia` reproduce el aspecto actual; `Equilibrado` es el candidato predeterminado tras las pruebas; `Detalle` aplica solo funciones validadas. `Restablecer` vuelve al perfil elegido.

| Control | Aplicación prevista | Dependencia o límite |
| --- | --- | --- |
| Materiales realistas | Intercambiar materiales cocinados PBR/color base. | Puede recargar la malla si cambian las secciones. |
| Relieve de superficies | Multiplicador del mapa normal. | Solo aparece activo si hay textura normal y tangentes válidas. |
| Oclusión del modelo | Intensidad de la textura AO del GLB. | No equivale a SSAO ni afecta sombras directas. |
| Metales y rugosidad | Usar los mapas y factores glTF. | Depende del material PBR cocinado. |
| Emisión | Intensidad de materiales emisivos. | No convierte la superficie en una luz que ilumine la habitación. |
| Luz ambiental / luz de escena | Activar y ajustar luces ya creadas para el modelo. | Límites de intensidad para evitar lavado o contraste extremo. |
| Sombras direccionales | Activar la luz con sombras y distancia limitada. | Oculto o deshabilitado hasta validar soporte, aspecto y rendimiento. |
| Detalle de texturas | Bajo, medio o alto. | Puede exigir recargar el modelo; mostrarlo antes de aplicar. |

Persistir perfil y valores en un archivo pequeño bajo `Houses/`; validar rangos al leerlo y mantener la URL del hub aparte. Los controles imposibles para un GLB concreto deben indicar `No disponible en este modelo`, en vez de fingir que cambiaron la imagen. No ofrecer un botón `Todo activado` que exceda el presupuesto de rendimiento.

## Medición y criterios de entrega

- Meta inicial: 72 FPS sostenidos en Quest 3S durante un recorrido de 15 minutos; a 72 Hz cada fotograma dura 13,89 ms. Medir tiempo de GPU de la app, CPU, memoria, cuadros perdidos y temperatura con OVR Metrics Tool o Meta Quest Developer Hub. Ajustar el presupuesto con datos reales del dispositivo.
- Comparar capturas en los mismos puntos y con las mismas condiciones de cada GLB. Incluir paredes, suelo, cristales, materiales metálicos, rincones oscuros y bordes finos. Revisar ambos ojos dentro del visor, no solo la captura del escritorio.
- Probar todos los estados de menú y acordes: pulsación X sola, X+Y en distinto orden, A+B, gatillo mantenido, cambio de casa mientras hay descarga y retorno durante carga. Confirmar que nunca se desplaza el jugador con un menú visible.
- Registrar en `HouseSandbox:` qué perfil y recursos se aplicaron, qué mapas se omitieron y por qué. Verificar el despliegue solo con Quest conectada. Para cambios de C++, compilar Android sin cocinar como comprobación temprana; para ver materiales nuevos en Quest será imprescindible el empaquetado de prueba indicado en la etapa 7.

## Referencias técnicas

- [Rutas mobile forward y deferred de Unreal 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/mobile-rendering-and-shading-modes-for-unreal-engine).
- [Funciones de render móvil de Unreal 5.8](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference).
- [Especificación glTF 2.0 de Khronos](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
- [OVR Metrics Tool de Meta](https://developers.meta.com/vr/documentation/unreal/ts-ovrmetricstool/).
