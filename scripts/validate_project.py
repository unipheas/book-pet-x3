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


def check_x3_shared_spi() -> None:
    source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    helper = re.search(
        r"void beginDisplayHardware\(\) \{(?P<body>.*?)\n\}", source, re.S
    )
    if not helper or "BoardConfig::ACTIVE.sd.miso" not in helper.group("body"):
        raise SystemExit(
            "X3 display startup must attach the shared SD MISO pin"
        )
    if source.count("display.begin();") != 1:
        raise SystemExit(
            "All display startup paths must use beginDisplayHardware()"
        )
    if source.count("beginDisplayHardware();") < 3:
        raise SystemExit(
            "Setup, wake, and SD recovery must preserve the shared SPI wiring"
        )


def check_updater_fails_closed() -> None:
    source = (ROOT / "src" / "FirmwareUpdater.cpp").read_text(encoding="utf-8")
    if "Update.installSignature" in source:
        raise SystemExit(
            "Do not use the framework signing hook; it clears its signature size"
        )
    required = (
        "totalBytes - kSignatureBytes",
        "signatureSha256_.add",
        "releaseVerifier().verify",
        'setError("The firmware signature is not trusted")',
    )
    if any(token not in source for token in required):
        raise SystemExit("Signed updates must retain and verify their RSA signature")
    if source.index("releaseVerifier().verify") > source.index("Update.end()"):
        raise SystemExit("The RSA signature must be verified before OTA activation")


def check_boot_release_guard() -> None:
    source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    setup = source[source.index("void setup()") : source.index("void loop()")]
    loop = source[source.index("void loop()") :]
    guard = "freeink::PowerManager::waitForPowerButtonRelease();"
    if guard not in setup or setup.index(guard) < setup.index("render(true);"):
        raise SystemExit(
            "Boot must consume the wake press after rendering recovery"
        )
    if "buttons.update();" not in setup[setup.index(guard) :]:
        raise SystemExit("Input state must be refreshed after the wake press")
    if "confirmRunningImage()" in setup:
        raise SystemExit(
            "A new OTA slot must not be confirmed immediately in setup"
        )
    if "confirmHealthyUpdateIfDue(true)" in source:
        raise SystemExit(
            "Sleep must not bypass the OTA healthy-runtime window"
        )
    if "OTA_HEALTHY_RUNTIME_MS = 5'000" not in source:
        raise SystemExit("OTA boot confirmation needs a healthy runtime window")
    if "confirmHealthyUpdateIfDue();" not in loop:
        raise SystemExit("The main loop must confirm a healthy OTA slot")


def check_update_portal_hardening() -> None:
    source = (ROOT / "src" / "UpdatePortal.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src" / "UpdatePortal.h").read_text(encoding="utf-8")
    trust = (ROOT / "src" / "UpdateTrust.h").read_text(encoding="utf-8")
    if "if (!routesConfigured_)" not in source or "routesConfigured_" not in header:
        raise SystemExit(
            "Phone update routes must only be registered once per boot"
        )
    if "character < 0x20" not in source:
        raise SystemExit("Phone update JSON must escape control characters")
    required = (
        "officialRunning_",
        "updateBusy()",
        "tokenAllowed()",
        "sessionToken_",
        "char password[19]",
        "WiFi.disconnect(false, true)",
        "WiFi.disconnect(true, true)",
    )
    if any(token not in source and token not in header for token in required):
        raise SystemExit(
            "Phone updates must serialize requests, use a session token, "
            "and clear station connections"
        )
    if 'R"BOOKPET_CA(-----BEGIN CERTIFICATE-----' not in trust:
        raise SystemExit("The HTTPS trust anchor must begin at the PEM header")


def check_release_output_allowlist() -> None:
    source = (ROOT / "scripts" / "package_release.py").read_text(
        encoding="utf-8"
    )
    if "MANAGED_OUTPUTS" not in source or "unexpected release files" not in source:
        raise SystemExit(
            "Release packaging must reject unexpected files in dist"
        )


def check_reader_invariants() -> None:
    config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
    main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    state = (ROOT / "src" / "PetState.h").read_text(encoding="utf-8")
    progress = (ROOT / "src" / "ReadingProgress.cpp").read_text(
        encoding="utf-8"
    )
    reader = (ROOT / "src" / "BookReader.cpp").read_text(encoding="utf-8")
    docs = (
        (ROOT / "README.md").read_text(encoding="utf-8")
        + (ROOT / "docs" / "BOOKS.md").read_text(encoding="utf-8")
    )
    workflow = (
        ROOT / ".github" / "workflows" / "build.yml"
    ).read_text(encoding="utf-8")
    required_config = (
        "-DFREEINK_BOOK_SMALL=1",
        "FreeInkBook=symlink://freeink-sdk/libs/book/FreeInkBook",
        "FreeInkUI=symlink://freeink-sdk/libs/ui/FreeInkUI",
    )
    if any(token not in config for token in required_config):
        raise SystemExit(
            "The X3 reader must use FreeInkBook's small-memory profile"
        )
    required_navigation = (
        '"MY PET"',
        '"BOOKS"',
        '"PET NOOK"',
        '"PET SETTINGS"',
        "Screen::Library",
        "Screen::Reader",
    )
    if any(token not in main for token in required_navigation):
        raise SystemExit("The v1 two-section reader navigation has drifted")
    forbidden = ("LOG PAGES", "FINISH THIS BOOK", "PAGE FRAGMENTS")
    if any(token in main for token in forbidden):
        raise SystemExit("Manual reading or fragment UI returned unexpectedly")
    if "uint32_t version = 6;" not in state:
        raise SystemExit("Pet save migration version must remain explicit")
    if "isForwardReadingPosition" not in progress:
        raise SystemExit("Reading rewards must reject duplicate page locations")
    reader_safety = (
        "uint64_t BookReader::hashContainer",
        "cache_.cancelWrite()",
        "Chapter cache could not be rebuilt",
        "buildStorage_",
    )
    if any(token not in reader for token in reader_safety):
        raise SystemExit(
            "EPUB identity, cache recovery, or X3 memory lending has drifted"
        )
    if (
        "pendingTransaction" not in progress
        or "completeReadingTransaction" not in main
        or "const ReadingProgressState before = state_;" not in progress
        or "state_.currentBookId = previousCurrentBookId;" not in progress
    ):
        raise SystemExit(
            "Reading rewards and retryable progress must remain interruption-safe"
        )
    if (
        "SPIFFS.begin(!storageInitialized)" not in progress
        or 'kStorageInitializedKey[] = "rfs_init"' not in progress
        or 'snprintf(out, cap, "/r%c%s"' not in progress
        or "SPIFFS.mkdir" in progress
    ):
        raise SystemExit(
            "Per-book progress must use flat SPIFFS and fail closed after initialization"
        )
    if (
        "bookProgressChecksum" not in progress
        or "Reading progress record is damaged" not in progress
    ):
        raise SystemExit("Per-book progress records must detect corruption")
    file_filter = (ROOT / "src" / "ReaderFileFilter.h").read_text(
        encoding="utf-8"
    )
    if (
        "name[0] == '.'" not in file_filter
        or 'isReadableEpubName("._The Little Prince.epub")' not in file_filter
    ):
        raise SystemExit(
            "Library scanning must ignore macOS AppleDouble EPUB metadata"
        )
    if (
        "skipped invalid EPUB" not in reader
        or "No readable EPUB files were found" not in reader
    ):
        raise SystemExit("A bad EPUB must not poison the rest of the library")
    if "last eight" in docs.lower() or "up to eight" in docs.lower():
        raise SystemExit("Reader documentation still describes the old LRU")
    if "FreeInkBook/test/host/run.sh" not in workflow:
        raise SystemExit("CI must run the FreeInkBook EPUB regression suite")


def check_release_workflow_security() -> None:
    release = (
        ROOT / ".github" / "workflows" / "release.yml"
    ).read_text(encoding="utf-8")
    build = (
        ROOT / ".github" / "workflows" / "build.yml"
    ).read_text(encoding="utf-8")
    site = (ROOT / "site" / "index.html").read_text(encoding="utf-8")
    required_release = (
        "environment: release-signing",
        "book-pet-unsigned-release",
        "book-pet-signed-update",
        "--signed-update",
        "--factory-image",
        "git merge-base --is-ancestor",
        "persist-credentials: false",
        "--require-hashes",
        "ffce4a512581abd417c42edf2695a3b49e8b1447849847d3f62d0db695da9efc",
        "workflow_dispatch:",
        "RELEASE_TAG",
    )
    if any(token not in release for token in required_release):
        raise SystemExit(
            "Release signing must run as a separate protected job"
        )
    if ".pio/build/xteink_x3_release/firmware.bin" not in build:
        raise SystemExit(
            "CI must publish the signature-enforcing firmware artifact"
        )
    vendor = ROOT / "site" / "vendor" / "esp-web-tools-10.4.0"
    if (
        'src="vendor/esp-web-tools-10.4.0/install-button.js"' not in site
        or "https://unpkg.com" in site
        or "script-src 'self'" not in site
        or not (vendor / "install-button.js").is_file()
        or not (vendor / "install-dialog-im156JnI.js").is_file()
        or not (vendor / "LICENSE").is_file()
    ):
        raise SystemExit(
            "The complete web installer dependency must be pinned and self-hosted"
        )


def main() -> None:
    check_versions()
    check_trust_material()
    check_partitions()
    check_no_private_key()
    check_x3_shared_spi()
    check_updater_fails_closed()
    check_boot_release_guard()
    check_update_portal_hardening()
    check_release_output_allowlist()
    check_reader_invariants()
    check_release_workflow_security()
    print("Book Pet release invariants are valid")


if __name__ == "__main__":
    main()
