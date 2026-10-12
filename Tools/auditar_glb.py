"""Auditoría de compatibilidad GLB para el importador VR; solo lectura."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

LIMITE_ARCHIVO = 100 * 1024 * 1024
LIMITE_TRIANGULOS = 350_000


def auditar(ruta: Path) -> bool:
    problemas: list[str] = []
    avisos: list[str] = []
    try:
        tamano = ruta.stat().st_size
        with ruta.open("rb") as archivo:
            cabecera = archivo.read(20)
            if len(cabecera) != 20:
                raise ValueError("cabecera GLB incompleta")
            firma, version, declarado, largo_json, tipo_json = struct.unpack("<4sIIII", cabecera)
            if firma != b"glTF" or version != 2 or tipo_json != 0x4E4F534A:
                raise ValueError("se necesita GLB 2.0 con bloque JSON inicial")
            if declarado != tamano:
                raise ValueError(f"longitud declarada {declarado} distinta de {tamano}")
            if largo_json > tamano - 28:
                raise ValueError("bloque JSON fuera del archivo")
            documento = json.loads(archivo.read(largo_json))
            cabecera_bin = archivo.read(8)
            if len(cabecera_bin) != 8:
                raise ValueError("falta bloque BIN")
            largo_bin, tipo_bin = struct.unpack("<II", cabecera_bin)
            if tipo_bin != 0x004E4942 or archivo.tell() + largo_bin > tamano:
                raise ValueError("bloque BIN inválido")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR {ruta}: {error}")
        return False

    if tamano > LIMITE_ARCHIVO:
        problemas.append("supera 100 MiB")
    if documento.get("asset", {}).get("version") != "2.0":
        problemas.append("asset.version no es 2.0")

    accesores = documento.get("accessors", [])
    materiales = documento.get("materials", [])
    imagenes = documento.get("images", [])
    triangulos = 0
    primitivas = 0
    for malla in documento.get("meshes", []):
        for primitiva in malla.get("primitives", []):
            primitivas += 1
            if "KHR_draco_mesh_compression" in primitiva.get("extensions", {}):
                problemas.append("contiene compresión Draco")
            if primitiva.get("mode", 4) != 4:
                avisos.append("hay primitivas que no son TRIANGLES")
                continue
            indice = primitiva.get("indices")
            posicion = primitiva.get("attributes", {}).get("POSITION")
            numero = indice if indice is not None else posicion
            if isinstance(numero, int) and 0 <= numero < len(accesores):
                triangulos += int(accesores[numero].get("count", 0)) // 3
            else:
                problemas.append("una primitiva carece de posiciones o índices válidos")
    if triangulos > LIMITE_TRIANGULOS:
        problemas.append("supera el límite de triángulos incluso antes de duplicar caras")
    elif triangulos * 2 > LIMITE_TRIANGULOS:
        avisos.append("la duplicación de caras puede omitir geometría por límite de triángulos")

    for imagen in imagenes:
        if "uri" in imagen:
            avisos.append("hay imágenes externas que el importador no carga")
        if imagen.get("mimeType") not in ("image/png", "image/jpeg", None):
            avisos.append("hay imágenes que no son PNG/JPEG")
    for material in materiales:
        pbr = material.get("pbrMetallicRoughness", {})
        for nombre, info in (("color base", pbr.get("baseColorTexture")),
                             ("metálico/rugosidad", pbr.get("metallicRoughnessTexture")),
                             ("normal", material.get("normalTexture")),
                             ("oclusión", material.get("occlusionTexture")),
                             ("emisión", material.get("emissiveTexture"))):
            if isinstance(info, dict) and info.get("texCoord", 0) != 0:
                avisos.append(f"un mapa de {nombre} usa TEXCOORD_{info['texCoord']}, aún no compatible")
    for extension in documento.get("extensionsUsed", []):
        if extension.startswith("KHR_materials_") and extension != "KHR_materials_emissive_strength":
            avisos.append(f"la extensión {extension} todavía no se reproduce por completo")

    print(f"{ruta.name}: {tamano / 1048576:.1f} MiB, {primitivas} primitivas, "
          f"{triangulos} triángulos de origen, {len(materiales)} materiales, {len(imagenes)} imágenes")
    for problema in sorted(set(problemas)):
        print(f"  ERROR: {problema}")
    for aviso in sorted(set(avisos)):
        print(f"  AVISO: {aviso}")
    return not problemas


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rutas", nargs="*", type=Path, help="Archivos .glb o carpetas; por defecto Tools/models")
    opciones = parser.parse_args()
    entradas = opciones.rutas or [Path(__file__).resolve().parent / "models"]
    archivos = sorted({archivo for entrada in entradas
                       for archivo in (entrada.glob("*.glb") if entrada.is_dir() else [entrada])})
    if not archivos:
        print("No se encontraron archivos GLB")
        return 1
    resultados = [auditar(archivo) for archivo in archivos]
    return 0 if all(resultados) else 1


if __name__ == "__main__":
    raise SystemExit(main())
