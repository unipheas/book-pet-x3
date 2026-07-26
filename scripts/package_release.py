#!/usr/bin/env python3
"""Create signed OTA, factory-flash, release, and web-installer artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from sign_firmware import sign_firmware

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_KEY = ROOT / "keys" / "book-pet-x3-update-public.pem"
MAX_UPDATE_BYTES = 0x680000


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def platformio_core_dir() -> Path:
    override = os.environ.get("PLATFORMIO_CORE_DIR")
    return Path(override).expanduser() if override else Path.home() / ".platformio"


def esptool_command() -> list[str]:
    core = platformio_core_dir()
    if os.name == "nt":
        python = core / "penv" / "Scripts" / "python.exe"
    else:
        python = core / "penv" / "bin" / "python"
    script = core / "packages" / "tool-esptoolpy" / "esptool.py"
    if python.is_file() and script.is_file():
        return [str(python), str(script)]
    try:
        __import__("esptool")
        return [sys.executable, "-m", "esptool"]
    except ImportError as error:
        raise SystemExit(
            "esptool was not found. Run the PlatformIO build first."
        ) from error


def require(path: Path) -> Path:
    if not path.is_file():
        raise SystemExit(f"Required build file not found: {path}")
    return path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=ROOT / ".pio" / "build" / "xteink_x3_release",
    )
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    parser.add_argument(
        "--base-url",
        default="https://unipheas.github.io/book-pet-x3/",
    )
    args = parser.parse_args()

    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if not version or any(char not in "0123456789." for char in version):
        raise SystemExit(f"Invalid VERSION: {version!r}")

    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    app = require(build / "firmware.bin")
    bootloader = require(build / "bootloader.bin")
    partitions = require(build / "partitions.bin")
    boot_app0 = require(
        platformio_core_dir()
        / "packages"
        / "framework-arduinoespressif32"
        / "tools"
        / "partitions"
        / "boot_app0.bin"
    )

    raw_app = output / "book-pet-x3.bin"
    update = output / "book-pet-x3-update.bin"
    factory = output / "book-pet-x3-factory.bin"
    shutil.copy2(app, raw_app)
    shutil.copy2(bootloader, output / "bootloader.bin")
    shutil.copy2(partitions, output / "partitions.bin")
    shutil.copy2(boot_app0, output / "boot_app0.bin")
    sign_firmware(app, args.private_key.resolve(), PUBLIC_KEY, update)
    if update.stat().st_size > MAX_UPDATE_BYTES:
        raise SystemExit("Signed update does not fit an OTA partition")

    subprocess.run(
        esptool_command()
        + [
            "--chip",
            "esp32c3",
            "merge-bin",
            "-o",
            str(factory),
            "--flash-mode",
            "dio",
            "--flash-freq",
            "40m",
            "--flash-size",
            "16MB",
            "0x0",
            str(bootloader),
            "0x8000",
            str(partitions),
            "0xe000",
            str(boot_app0),
            "0x10000",
            str(app),
        ],
        check=True,
    )

    base_url = args.base_url.rstrip("/") + "/"
    update_digest = sha256(update)
    (output / f"{update.name}.sha256").write_text(
        f"{update_digest}  {update.name}\n", encoding="utf-8"
    )
    stable = {
        "schema": 1,
        "product": "book-pet-x3",
        "version": version,
        "url": base_url + update.name,
        "sha256": update_digest,
        "size": update.stat().st_size,
    }
    (output / "stable.json").write_text(
        json.dumps(stable, indent=2) + "\n", encoding="utf-8"
    )
    manifest = {
        "name": "Book Pet X3",
        "version": version,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [
            {
                "chipFamily": "ESP32-C3",
                "parts": [{"path": factory.name, "offset": 0}],
            }
        ],
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )

    artifacts = sorted(
        path
        for path in output.iterdir()
        if path.is_file() and path.name != "SHA256SUMS"
    )
    checksums = "".join(f"{sha256(path)}  {path.name}\n" for path in artifacts)
    (output / "SHA256SUMS").write_text(checksums, encoding="utf-8")
    print(f"Packaged Book Pet {version} in {output}")


if __name__ == "__main__":
    main()
