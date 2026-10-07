"""Comprueba las huellas SHA-256 del release de Quest sin modificar archivos."""

from __future__ import annotations

import hashlib
from pathlib import Path


def main() -> int:
    carpeta = Path(__file__).resolve().parent.parent / "deploy" / "releases" / "quest3s-test-actual"
    manifiesto = carpeta / "SHA256SUMS.txt"
    if not manifiesto.is_file():
        print(f"Falta {manifiesto}")
        return 1
    errores = 0
    for linea in manifiesto.read_text(encoding="utf-8-sig").splitlines():
        esperado, separador, nombre = linea.partition("  ")
        if not separador or len(esperado) != 64:
            print(f"Línea inválida: {linea}")
            errores += 1
            continue
        ruta = (carpeta / nombre).resolve()
        if not ruta.is_relative_to(carpeta.resolve()) or not ruta.is_file():
            print(f"Falta o sale del release: {nombre}")
            errores += 1
            continue
        huella = hashlib.sha256()
        with ruta.open("rb") as archivo:
            for bloque in iter(lambda: archivo.read(1024 * 1024), b""):
                huella.update(bloque)
        if huella.hexdigest() != esperado.lower():
            print(f"NO COINCIDE: {nombre}")
            errores += 1
        else:
            print(f"OK: {nombre}")
    return 1 if errores else 0


if __name__ == "__main__":
    raise SystemExit(main())
