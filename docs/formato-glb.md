# Contrato de modelos GLB

## Exportación desde 3ds Max

1. Prepara la casa como mallas estáticas triangulables y revisa unidades y escala. El visor interpreta 1 unidad glTF como 1 metro.
2. Incluye UV y texturas de color base PNG o JPEG **dentro** de un archivo GLB 2.0. El importador no busca imágenes externas.
3. Coloca un suelo transitable reconocible; el importador estima el suelo automáticamente y centra el conjunto en X/Y.
4. Exporta sin Draco, animación ni extensiones que cambien la geometría base. Haz una prueba con una casa pequeña antes de subir una escena completa.
5. Para que se vean sombras oclusivas con el material actual, hornéalas en la imagen de color base durante la preparación del activo. La app no interpreta mapas AO separados.

El archivo nativo `.max` no se abre en Quest. Debe salir de 3ds Max como `.glb`. Un `.gltf` con texturas separadas tampoco entra en el flujo actual.

## Datos que se leen

- GLB 2.0 con un bloque JSON y un bloque BIN.
- Nodos de la escena activa con traslación, rotación, escala o matriz.
- Primitivas `TRIANGLES`, posiciones flotantes, índices enteros, normales y `TEXCOORD_0`.
- `baseColorTexture` PNG/JPEG embebida, `baseColorFactor` y `alphaMode` básico.

## Datos que no se reproducen fielmente

- PBR completo: normales de textura, metálico, rugosidad, AO, emisivo y reflejos específicos del modelo.
- Draco, KTX2, texturas externas, animaciones, esqueletos, morph targets y variantes de material.
- UV de canales distintos de `TEXCOORD_0` y extensiones específicas de exportadores.

El modo `MASK` o `BLEND` utiliza materiales existentes, pero la apariencia final puede diferir del archivo original. Materiales sin una imagen de color base reciben una textura blanca de respaldo.

## Diagnóstico rápido

| Síntoma | Revisión |
| --- | --- |
| Modelo demasiado alto o pequeño | Comprueba las unidades antes de exportar; 1 m GLB equivale a 100 cm Unreal. |
| Jugador debajo o fuera | Revisa el suelo, geometría muy lejana al origen y objetos decorativos grandes. |
| Superficie blanca o sin detalle | Confirma que el PNG/JPEG de color base está embebido y tiene UV0. |
| Malla incompleta | Revisa el límite de triángulos, el modo `TRIANGLES` y la ausencia de Draco. |
| Pausa prolongada al abrir | Reduce polígonos, número de materiales y tamaño de texturas. |

Consulta `docs/arquitectura.md` para límites exactos y `docs/operacion.md` para leer el log del visor.
