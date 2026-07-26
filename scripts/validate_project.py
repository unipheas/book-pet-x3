#!/usr/bin/env python3
"""Fail CI when release trust, versioning, or flash layout drifts."""

from __future__ import annotations

import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def extract_pem(path: Path, kind: str) -> str:
    text = path.read_text(encoding="utf-8")
    match = re.search(
        rf"(-----BEGIN {kind}-----.*?-----END {kind}-----)", text, re.S
    )
    if not match:
        raise SystemExit(f"{path} does not contain a {kind} PEM block")
    return match.group(1).strip() + "\n"


def check_versions() -> None:
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    header = (ROOT / "src" / "BookPetVersion.h").read_text(encoding="utf-8")
    site_manifest = json.loads(
        (ROOT / "site" / "manifest.json").read_text(encoding="utf-8")
    )
    if f'"{version}"' not in header:
        raise SystemExit("VERSION and BookPetVersion.h do not match")
    if site_manifest.get("version") != version:
        raise SystemExit("VERSION and site/manifest.json do not match")
    builds = site_manifest.get("builds", [])
    if len(builds) != 1 or builds[0].get("chipFamily") != "ESP32-C3":
        raise SystemExit("Web installer must target only ESP32-C3")
    parts = builds[0].get("parts", [])
    if parts != [{"path": "book-pet-x3-factory.bin", "offset": 0}]:
        raise SystemExit("Web installer must flash the merged factory image at 0")


def check_trust_material() -> None:
    public_header = extract_pem(ROOT / "src" / "UpdatePublicKey.h", "PUBLIC KEY")
    public_file = (
        ROOT / "keys" / "book-pet-x3-update-public.pem"
    ).read_text(encoding="utf-8")
    if public_header.strip() != public_file.strip():
        raise SystemExit("Embedded and release public keys do not match")

    root_ca = extract_pem(ROOT / "src" / "UpdateTrust.h", "CERTIFICATE")
    with tempfile.TemporaryDirectory(prefix="book-pet-trust-") as temp:
        public_path = Path(temp) / "public.pem"
        ca_path = Path(temp) / "root.pem"
        public_path.write_text(public_header, encoding="utf-8")
        ca_path.write_text(root_ca, encoding="utf-8")
        key = subprocess.run(
            ["openssl", "pkey", "-pubin", "-in", public_path, "-text", "-noout"],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        ).stdout
        if "4096 bit" not in key:
            raise SystemExit("Update public key must be RSA-4096")
        certificate = subprocess.run(
            ["openssl", "x509", "-in", ca_path, "-noout", "-text"],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        ).stdout
        if "CA:TRUE" not in certificate:
            raise SystemExit("Update HTTPS trust anchor is not a valid CA")
        subprocess.run(
            ["openssl", "x509", "-in", ca_path, "-noout", "-checkend", "31536000"],
            check=True,
        )


def check_partitions() -> None:
    expected = {
        "nvs": (0x9000, 0x5000),
        "otadata": (0xE000, 0x2000),
        "app0": (0x10000, 0x680000),
        "app1": (0x690000, 0x680000),
        "spiffs": (0xD10000, 0x2E0000),
        "coredump": (0xFF0000, 0x10000),
    }
    actual: dict[str, tuple[int, int]] = {}
    for raw in (ROOT / "partitions.csv").read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = [field.strip() for field in line.split(",")]
        actual[fields[0]] = (int(fields[3], 0), int(fields[4], 0))
    if actual != expected:
        raise SystemExit("partitions.csv changed; review OTA and NVS safety first")
    ordered = sorted((offset, offset + size, name) for name, (offset, size) in actual.items())
    for previous, current in zip(ordered, ordered[1:]):
        if previous[1] > current[0]:
            raise SystemExit(
                f"Partition overlap: {previous[2]} and {current[2]}"
            )
    if ordered[-1][1] != 16 * 1024 * 1024:
        raise SystemExit("Partition table does not end at the 16 MB boundary")


def check_no_private_key() -> None:
    ignored = {".git", ".pio", "dist", "__pycache__", "freeink-sdk"}
    marker = "-----BEGIN " + "PRIVATE KEY-----"
    for path in ROOT.rglob("*"):
        if not path.is_file() or any(part in ignored for part in path.parts):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        if marker in text:
            raise SystemExit(f"Private key material found in repository: {path}")


def main() -> None:
    check_versions()
    check_trust_material()
    check_partitions()
    check_no_private_key()
    print("Book Pet release invariants are valid")


if __name__ == "__main__":
    main()
