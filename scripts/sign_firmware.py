#!/usr/bin/env python3
"""Append and verify the RSA-4096 signature expected by Arduino Update."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import tempfile
from pathlib import Path

SIGNATURE_BYTES = 512


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def sign_firmware(
    source: Path, private_key: Path, public_key: Path, destination: Path
) -> str:
    if not source.is_file():
        raise SystemExit(f"Firmware not found: {source}")
    if source.stat().st_size < 32 * 1024:
        raise SystemExit("Firmware is unexpectedly small")
    if not private_key.is_file():
        raise SystemExit(f"Private key not found: {private_key}")
    if not public_key.is_file():
        raise SystemExit(f"Public key not found: {public_key}")
    if not shutil.which("openssl"):
        raise SystemExit("OpenSSL is required to sign release firmware")

    key_details = run(
        ["openssl", "pkey", "-in", str(private_key), "-text", "-noout"]
    )
    if "4096 bit" not in key_details.stdout:
        raise SystemExit("The release key must be RSA-4096")

    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="book-pet-sign-") as temp:
        signature = Path(temp) / "signature.bin"
        run(
            [
                "openssl",
                "dgst",
                "-sha256",
                "-sign",
                str(private_key),
                "-out",
                str(signature),
                str(source),
            ]
        )
        signature_bytes = signature.read_bytes()
        if len(signature_bytes) != SIGNATURE_BYTES:
            raise SystemExit(
                f"Expected a {SIGNATURE_BYTES}-byte RSA signature, "
                f"got {len(signature_bytes)}"
            )
        destination.write_bytes(source.read_bytes() + signature_bytes)
        run(
            [
                "openssl",
                "dgst",
                "-sha256",
                "-verify",
                str(public_key),
                "-signature",
                str(signature),
                str(source),
            ]
        )

    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    print(
        f"Signed {source.name} -> {destination.name} "
        f"({destination.stat().st_size} bytes, sha256 {digest})"
    )
    return digest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--public-key", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    sign_firmware(
        args.input.resolve(),
        args.private_key.resolve(),
        args.public_key.resolve(),
        args.output.resolve(),
    )


if __name__ == "__main__":
    main()
