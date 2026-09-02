#!/usr/bin/env python3
"""Release-Binaries bauen und firmware/manifest.json aktualisieren.

Der bisherige Handbetrieb (bauen, md5sum, kopieren, Manifest von Hand
pflegen) war mit einer Variante schon fehleranfaellig und wird mit zweien
unzumutbar: ein vergessener MD5 heisst, dass jede Waage das Update
herunterlaedt, pruefen laesst und verwirft - sichtbar wird das erst am
Geraet. Deshalb macht das hier ein Skript, das immer alle vier Werte
(Version, Datei, Groesse, MD5) gemeinsam neu schreibt.

Aufruf (aus dem Repo-Wurzelverzeichnis):

    python scripts/release.py               # beide Varianten
    python scripts/release.py t-oi-plus      # nur die Basis
    python scripts/release.py --no-build    # nur Manifest aus vorhandenen .bin

Die Version kommt aus FIRMWARE_VERSION in include/Config.h - sie wird NICHT
automatisch hochgezaehlt, das bleibt eine bewusste Entscheidung vor dem
Release.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CONFIG_H = REPO_ROOT / "include" / "Config.h"
FIRMWARE_DIR = REPO_ROOT / "firmware"
MANIFEST = FIRMWARE_DIR / "manifest.json"

# Schluessel = das, was die Waage per BLE als "variant" meldet (siehe
# BleWeightService::buildDeviceInfoJson()); genau danach sucht die App im
# Manifest.
VARIANTS = {
    "t-display-s3": {
        "env": "t-display-s3",
        "name": "Massarbeit Vision",
        "bin": "firmware/t-display-s3.bin",
    },
    "t-oi-plus": {
        "env": "t-oi-plus",
        "name": "Massarbeit",
        "bin": "firmware/t-oi-plus.bin",
    },
}

# Diese Variante spiegelt sich zusaetzlich in die Top-Level-Felder des
# Manifests (version/file/size/md5). Reine Rueckwaertskompatibilitaet: ein
# alter, im Browser gecachter App-Stand kennt "variants" noch nicht und liest
# nur die Top-Level-Felder - der soll weiterhin die Vision aktualisieren
# koennen, statt am neuen Manifest zu scheitern.
LEGACY_VARIANT = "t-display-s3"


def read_firmware_version() -> str:
    text = CONFIG_H.read_text(encoding="utf-8")
    match = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', text)
    if not match:
        sys.exit(f"FIRMWARE_VERSION nicht in {CONFIG_H} gefunden.")
    return match.group(1)


def build(env: str) -> None:
    print(f"[release] Baue Environment '{env}' ...")
    result = subprocess.run(["pio", "run", "-e", env], cwd=REPO_ROOT)
    if result.returncode != 0:
        sys.exit(f"Build von '{env}' fehlgeschlagen.")


def md5_of(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def publish(variant: str, version: str, do_build: bool) -> dict:
    spec = VARIANTS[variant]
    if do_build:
        build(spec["env"])

    built = REPO_ROOT / ".pio" / "build" / spec["env"] / "firmware.bin"
    target = REPO_ROOT / spec["bin"]

    if do_build or not target.exists():
        if not built.exists():
            sys.exit(f"Keine gebaute Firmware unter {built} - erst ohne --no-build laufen lassen.")
        FIRMWARE_DIR.mkdir(exist_ok=True)
        shutil.copyfile(built, target)

    entry = {
        "version": version,
        "name": spec["name"],
        "file": spec["bin"],
        "size": target.stat().st_size,
        "md5": md5_of(target),
    }
    print(f"[release] {variant}: {entry['size']} Bytes, MD5 {entry['md5']}")
    return entry


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "variants",
        nargs="*",
        choices=list(VARIANTS),
        help="Welche Varianten veroeffentlicht werden (Vorgabe: alle).",
    )
    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Nicht neu bauen, nur Groesse/MD5 der vorhandenen firmware/*.bin ins Manifest schreiben.",
    )
    args = parser.parse_args()

    selected = args.variants or list(VARIANTS)
    version = read_firmware_version()
    print(f"[release] FIRMWARE_VERSION = {version}")

    # Bestehendes Manifest weiterschreiben statt neu erfinden: wer nur eine
    # Variante baut, soll den Eintrag der anderen nicht verlieren.
    manifest = {}
    if MANIFEST.exists():
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    variant_entries = manifest.get("variants", {})

    # ... aber nur fuer Varianten, die es noch gibt. VARIANTS oben ist die
    # Wahrheit darueber, welche Modelle existieren; ein Eintrag fuer ein
    # zurueckgezogenes Modell wuerde sonst ewig im Manifest stehen bleiben und
    # auf eine .bin zeigen, die im Repo laengst geloescht ist.
    for retired in [key for key in variant_entries if key not in VARIANTS]:
        print(f"[release] Entferne Manifest-Eintrag fuer unbekannte Variante '{retired}'.")
        variant_entries.pop(retired)

    for variant in selected:
        variant_entries[variant] = publish(variant, version, do_build=not args.no_build)

    legacy = variant_entries.get(LEGACY_VARIANT)
    manifest = {
        "version": legacy["version"] if legacy else version,
        "file": legacy["file"] if legacy else "",
        "size": legacy["size"] if legacy else 0,
        "md5": legacy["md5"] if legacy else "",
        "variants": variant_entries,
    }

    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"[release] {MANIFEST.relative_to(REPO_ROOT)} geschrieben.")
    print("[release] Jetzt noch committen und auf main pushen.")


if __name__ == "__main__":
    main()
