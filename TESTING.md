# Testing Book Pet

Book Pet combines persistent embedded state, an EPUB engine, e-paper hardware,
and signed firmware updates. A compiler success is necessary, but it is not the
same as a hardware test. This document separates the verification layers so
release claims stay honest.

## Quick contributor check

From the repository root:

```sh
git submodule update --init --recursive
python3 scripts/validate_project.py
freeink-sdk/libs/book/FreeInkBook/test/host/run.sh
pio run -e xteink_x3
```

Before a release or any change to update trust, partitions, persistence, or
reader memory:

```sh
pio run -e xteink_x3_release
```

## Verification layers

### Release invariants

`python3 scripts/validate_project.py` checks facts that must not drift silently:

- firmware, source header, and web-installer versions match;
- the installer targets only ESP32-C3 and the merged factory image;
- the partition table remains non-overlapping and exactly 16 MiB;
- public update keys and embedded keys match;
- the HTTPS trust anchor is a valid, non-expiring CA;
- no private signing key appears in tracked project content;
- X3 display startup preserves shared SD SPI wiring;
- update verification remains fail-closed;
- OTA confirmation keeps its healthy-runtime window;
- the phone portal keeps its authentication and credential-clearing guards;
- release output uses an allowlist;
- reader navigation, progress, cache recovery, macOS metadata filtering, and
  reward-journal invariants remain present;
- CI continues to build both firmware variants and run the EPUB host suite.

This validator is deliberately strict. If it fails, understand the invariant
before changing the check.

### FreeInkBook host suite

`freeink-sdk/libs/book/FreeInkBook/test/host/run.sh` builds the freestanding
reader engine with warnings treated as errors and assembles test EPUB fixtures.
It covers:

- stored and deflated ZIP entries;
- EPUB 2 and EPUB 3 package parsing;
- NCX and navigation documents;
- CSS and text layout;
- small, default, and large memory profiles;
- Unicode line breaking and hyphenation;
- PNG and JPEG rendering;
- page-cache reads and recovery;
- bitmap and TrueType font behavior;
- a generated 1,700-chapter catalog;
- malformed, truncated, and unsupported content paths.

The current suite reports 36,755 catalog checks plus the layout, cache, font,
and container suites. A progressive-JPEG case is skipped locally when Pillow is
not installed; CI and release evidence should state any skip rather than hiding
it.

### Firmware builds

`pio run -e xteink_x3` builds the developer firmware. It is suitable for USB
flashing and local iteration.

`pio run -e xteink_x3_release` builds the signature-enforcing firmware used by
the protected release workflow. Both must compile before a release. Watch:

- flash and RAM usage;
- newly introduced compiler warnings;
- changes in the pinned platform or library graph;
- accidental changes to partition or signing flags.

### Hardware checks

Use only an unlocked XTEINK X3 Developer/overseas edition. Record the panel
variant when known.

For reader changes:

1. Insert a FAT32 or exFAT card containing `/BOOKS/Test.epub`.
2. Include a macOS `._Test.epub` file or copy the book from Finder.
3. Open Books → Library and confirm only the real book appears.
4. Open the book and wait for first-time indexing.
5. Turn forward and backward with both button pairs.
6. Return to Library, reopen the book, and confirm resume.
7. Restart or sleep/wake and confirm resume again.
8. Revisit an old page and confirm it does not duplicate Page Bites or XP.
9. Finish a short fixture and confirm the completion reward is granted once.
10. Remove or corrupt a test EPUB and confirm a recoverable, accurate message.

For pet and display changes:

1. Verify portrait orientation and every physical button.
2. Exercise Feed, Play, Clean, and Rest.
3. Observe fast updates and a periodic full cleanup refresh.
4. Verify manual sleep and power-button wake.
5. Verify autonomous movement, drowsiness, natural sleep, and timer wake.
6. Confirm the previous pet save migrates without losing inventory or progress.

For update changes:

1. Install a correctly signed package from SD.
2. Install through the local browser portal.
3. Reject a deliberately corrupted signature without rebooting.
4. Boot the new inactive slot and wait for healthy confirmation.
5. Restore the previous slot without losing pet or reading progress.
6. Enter hold-Back recovery and confirm the screen remains usable.

## Test fixtures and real books

The host suite generates its own fixtures under the operating system's
temporary directory. Do not commit copyrighted commercial EPUBs. Hardware
testing should use:

- public-domain books;
- original contributor-created fixtures;
- files whose license permits redistribution;
- deliberately malformed local copies that are not published.

## Reporting results

Pull requests should state:

- exact commands run;
- pass counts and skipped cases;
- X3 model/edition and panel variant when known;
- whether the firmware was compiler-only or physically flashed;
- SD format and relevant EPUB traits;
- visible UI or recovery behavior checked;
- any long-duration battery or ghosting test still outstanding.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the pull-request checklist and
[docs/RELEASING.md](docs/RELEASING.md) for the protected release gate.
