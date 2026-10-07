# Trabajo en equipo y entregas

## Repositorio

La rama inicial es `main`. Los commits del historial inicial separan la base Unreal, el hub y modelos, la documentación y la automatización. No hay remoto configurado: los cambios permanecen locales hasta conectar un repositorio nuevo.

Los `.uasset`, `.umap`, `.glb`, el ejecutable grande del contenido y el release usan Git LFS. Después de clonar ejecuta `git lfs pull`. No sustituyas archivos LFS por punteros de texto en despliegues. `Saved/`, `Intermediate/`, `Binaries/`, `DerivedDataCache/`, `Tools/MaterialLab/` y logs de deploy siguen en el disco local, fuera de Git.

Antes de publicar los GLB y binarios en un repositorio público, confirma los permisos de distribución de los modelos descargados y de los recursos de terceros del proyecto. Los commits locales no publican nada por sí solos.

## Cambios futuros

1. Abre una rama por tarea y describe el comportamiento esperado.
2. Consulta `AGENTS.md`, `docs/arquitectura.md` y el código fuente antes de editar.
3. Mantén separadas modificaciones de importador, interfaz, hub, documentos y release.
4. Comprueba `git diff --check`, la auditoría de GLB cuando corresponda y el visor para cambios de VR.
5. Registra en la descripción del commit qué se probó y cualquier límite pendiente. Usa mensajes claros en español.
6. No actualices `deploy/releases/quest3s-test-actual` con un APK no verificado; crea una nueva carpeta de release y su manifiesto de hashes.

## Revisión mínima de una entrega Quest

- El APK y los cinco archivos `Content/Paks` coinciden con el manifiesto SHA-256.
- El panel abre, lista la biblioteca local y alcanza el hub.
- `demo_room.glb` y un GLB texturizado cargan a escala útil.
- El gatillo derecho funciona; X abre el panel; las dos palancas mantienen el movimiento continuo.
- La app no presenta superficies negras nuevas ni una caída perceptible de fluidez.

La instrucción vigente de no cocinar shaders nuevos se respeta en esta estructura. Si cambia, documenta la autorización y añade un flujo de compilación separado; no alteres el release probado.
