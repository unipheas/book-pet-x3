# Book Pet architecture

This document explains how Book Pet turns the XTEINK X3 Developer Edition into
an offline virtual pet and EPUB reader. It is written for contributors who want
to change the firmware without putting pet progress, books, or recovery at
risk.

## System overview

Book Pet is one ESP32-C3 application with four deliberately separate kinds of
state:

```text
Physical controls
       |
       v
  main.cpp screen and input state
       |
       +----------------------+----------------------+
       |                      |                      |
       v                      v                      v
  PetEngine              BookReader            Update system
  care and life          EPUB and pages         signed OTA/recovery
       |                      |                      |
       v                      v                      v
  Preferences/NVS       SD + internal flash     dual app slots
```

The separation is important:

- pet care and inventory do not depend on an SD card;
- books and rebuildable page caches stay on the SD card;
- reading progress uses a dedicated internal filesystem;
- firmware updates write the inactive application slot;
- normal updates and rollbacks do not erase pet or reading progress.

## Runtime components

### Application shell

`src/main.cpp` owns:

- startup and X3 panel detection;
- the portrait screen state machine;
- physical-button routing;
- pet, book, and update menus;
- display refresh cadence;
- reading reward coordination;
- manual and natural sleep;
- the main event loop.

The two top-level destinations are **My Pet** and **Books**. Pet Nook and Pet
Settings contain secondary pet features; Continue Reading, Library, and Reading
Rewards contain the reader flow.

### Pet engine

`src/PetState.cpp` and `src/PetState.h` own the persistent pet:

- needs, inventory, Page Bites, XP, levels, and species;
- care actions and Page Catch results;
- autonomous poses, sleep cycles, and deterministic thoughts;
- toy and pet unlocks;
- migrations from older saved-state versions;
- idempotent application of reading-reward transactions.

The pet is deterministic. There is no language model, remote inference, or
hidden network dependency.

### Reader adapter

`src/BookReader.cpp` and `src/BookReader.h` adapt FreeInkBook to the X3:

- mount the SD card and scan `/BOOKS`;
- ignore hidden filesystem metadata;
- identify books from ZIP central-directory contents;
- build or reopen EPUB catalogs;
- lay out chapters for a 528×792 portrait page;
- stream page data and images;
- store rebuildable caches under `/BOOKPET/CACHE`;
- lend the display's unused build buffer during memory-heavy indexing.

FreeInkBook remains inside the pinned `freeink-sdk` submodule. Book Pet supplies
the SD file source, cache storage, memory budgets, font, screen dimensions, and
user-facing error handling.

### Reading progress and reward journal

`src/ReadingProgress.cpp` stores two related records:

- one small progress file per content-derived book ID in SPIFFS;
- one current/pending transaction journal in Preferences/NVS.

Per-book files contain the resume position, furthest rewarded position, and
completion flag. Every v2 record has a checksum. Temp, backup, and target files
ensure an interrupted write leaves either the previous valid record or the new
valid record. A damaged target can recover from its backup; if neither copy is
valid, the reader fails closed instead of treating the book as unread and
granting its rewards again.

SPIFFS is allowed to format itself only before the first successful reading
storage initialization. That success is marked in NVS. Any later mount failure
is reported without formatting, protecting existing high-water marks and
completion records.

The reward sequence is intentionally ordered:

```text
1. Reader reaches a new stable EPUB location.
2. ReadingProgress records a pending transaction in NVS.
3. PetEngine applies the reward and saves the transaction ID with pet state.
4. ReadingProgress commits the new high-water mark and clears the journal.
5. If power fails between steps, startup replays the unfinished step.
```

`PetEngine` remembers the last transaction ID, so replaying step 3 cannot grant
the same reward twice. `ReadingProgress` does not advance the rewarded
high-water mark until the pet save has succeeded, so a failed pet save cannot
lose the reward.

### Display, input, and power

The pinned FreeInk SDK supplies:

- the X3 and X3 UC8279 board profiles;
- display-controller detection;
- one-bit framebuffer and e-paper drivers;
- six-button input abstraction;
- SD card management;
- deep-sleep wake selection;
- FreeInkUI's bitmap book font;
- the FreeInkBook engine.

Book Pet rotates the physical 792×528 panel counter-clockwise into a logical
528×792 portrait canvas. Fast monochrome updates are used for ordinary
interaction, with a full cleanup refresh after twelve visible updates and at
boot.

The X3 display and SD card share SPI clock and data lines. Book Pet attaches the
SD MISO pin before the display begins and restores that wiring after SD-update
operations. The X3 GPIO 13 soft-power latch remains app-owned because its
behavior was verified on hardware and is not yet represented by the upstream
board profile.

### Update and recovery system

`FirmwareUpdater`, `UpdatePortal`, and the update screens provide:

- signed SD updates;
- signed local-browser uploads;
- official HTTPS update checks;
- inactive-slot installation;
- five-second healthy-runtime confirmation;
- previous-slot rollback;
- hold-Back-at-boot recovery.

Official builds require an RSA-4096/SHA-256 signature appended to the
application image. The public key is embedded in the firmware; the private key
exists only in the protected GitHub release environment. Developer builds are
allowed to install unsigned local firmware for development.

## Storage map

| Location | Contents | Survives normal OTA | Safe to rebuild |
|---|---|---:|---:|
| NVS | Pet state and reading transaction journal | Yes | No |
| SPIFFS | Per-book resume/reward records | Yes | No |
| SD `/BOOKS` | User-owned EPUB files | Yes | No |
| SD `/BOOKPET/CACHE` | EPUB indexes and page caches | Yes | Yes |
| OTA app0/app1 | Current and previous firmware | One slot changes | Yes |
| SD `/BOOKPET/UPDATE.BIN` | Optional update package | Not applicable | Yes |

A clean factory install may erase internal flash, including NVS and SPIFFS.
Back up anything important before replacing unrelated firmware.

## EPUB identity

Book Pet does not use a filename as the progress key. It hashes the EPUB ZIP
central directory together with the archive geometry:

- renaming or moving the same EPUB into `/BOOKS` preserves its progress;
- replacing the file with a changed edition creates a different progress key;
- the entire book does not need to fit in RAM;
- cache directories remain deterministic across restarts.

Only ordinary, visible `.epub` files directly inside `/BOOKS` are scanned.
Hidden macOS AppleDouble files such as `._Book.epub` are ignored. Invalid,
truncated, and unsupported ZIP containers are skipped individually, so one bad
file cannot hide valid books or create an unsafe zero-ID catalog entry.

## Memory model

The ESP32-C3 has limited internal RAM and no PSRAM in this target. The reader
therefore:

- uses FreeInkBook's small-memory profile;
- streams ZIP entries instead of loading whole books;
- stores catalogs and page data on the SD card;
- keeps bounded catalogs and chapter buffers;
- temporarily borrows display build storage while indexing;
- fails with a user-facing error if a chapter cannot fit safely.

Do not increase reader arenas or resident catalog limits without measuring the
normal and release builds and testing complex books on hardware.

## Persistence compatibility

`PetState.version` controls pet-save migration. New fields must preserve
existing layouts or add an explicit migration. Reader progress has its own
versioned structures and must remain separate from the pet record.

Changes to any of the following require extra review:

- `partitions.csv`;
- NVS namespaces or keys;
- `PetState` or `ReadingProgressState` layout;
- reward transaction ordering;
- book identity;
- update signature or rollback behavior.

## Build and release flow

```text
source + pinned FreeInk SDK
          |
          +--> release invariant validator
          +--> FreeInkBook host suite
          +--> developer firmware build
          +--> signature-enforcing build
                         |
                         v
              protected signing job
                         |
                         v
         GitHub release + web installer + stable manifest
```

See [TESTING.md](TESTING.md) for verification commands and
[docs/RELEASING.md](docs/RELEASING.md) for the signed publishing process.
