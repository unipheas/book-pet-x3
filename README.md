# Book Pet

[![Release](https://img.shields.io/github/v/release/unipheas/book-pet-x3)](https://github.com/unipheas/book-pet-x3/releases/latest)
[![Build](https://github.com/unipheas/book-pet-x3/actions/workflows/build.yml/badge.svg)](https://github.com/unipheas/book-pet-x3/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A tiny, standalone portrait e-paper virtual pet for the **XTEINK X3 Developer
Edition**. Book Pet lives entirely on the device: no phone, Wi-Fi, Bluetooth,
account, cloud service, or AI connection is required for play. Wi-Fi is only
turned on when you deliberately open the update screen.

Meet Byte, Mote, and Pip: original 8-bit pixel familiars who eat pages. Feed
them, play with them, read real books to earn food and toys, level up, and carry
one on the back of your phone. The pet wanders around its room, becomes drowsy,
dreams, and can wake itself. Its state is saved locally, and its deliberate
stop-motion rhythm is designed around the strengths of e-paper.

> [!WARNING]
> Book Pet replaces the device firmware. It is intended only for the unlocked
> XTEINK X3 Developer/overseas edition. Do not flash it on an X4 or a
> USB-locked/restricted-market X3. Back up anything you care about first.

## Features

- Portrait 528×792 interface designed around the X3's physical controls
- Three original monochrome pets: Byte, Mote, and Pip
- Pet-specific idle, happy, and sleeping poses
- Feed, Play, Clean, and Rest actions
- Fullness, joy, rest, and cleanliness meters
- Page Bite currency, pantry, food inventory, experience, and levels
- Three-lane Page Catch memory game
- Story, Mystery, Science, and Adventure fragment collection
- Curious, Cozy, and Bold personalities shaped by care choices
- Hatchling, Sprout, and Familiar growth stages
- Three-entry diary of the current pet's recent life
- Deterministic on-device thoughts that reflect mood and recent events
- Reading log where every ten pages earns food
- Finished-book rewards that unlock toys and new pets
- Persistent toy box and equipped toys visible in the pet's room
- Visible autonomous wandering, reactions, drowsiness, sleep, dreams, and wake
- Dedicated Pet Life screen to explain or disable autonomous behavior
- Menu screens for reading, toys, pets, pantry, diary, and stats
- Persistent local state in ESP32 NVS
- Natural sleep after a quiet period, plus manual sleep
- All six page buttons mapped to navigation and actions
- Conservative fast refreshes with periodic full cleanup refreshes
- Complete offline operation
- One-click first installation from a supported desktop browser
- No-app phone updates through a temporary private X3 Wi-Fi network
- SD-card updates from `/BOOKPET/UPDATE.BIN`
- Official signed online updates with RAM-only Wi-Fi credentials
- Two firmware slots with previous-version rollback
- Hold-Back-at-boot recovery menu
- Reproducible PlatformIO build
- FreeInk-based X3 display, input, board detection, and sleep integration
- Hardware-tested release binaries

## Controls

- Front Left/Right: choose **Feed**, **Play**, **Clean**, or **Rest**
- Front Confirm: perform the selected action or select a menu item
- Front Back: open the menu or return to the previous screen
- Side Up/Down: move through menu items
- Hold power for about 1.2 seconds: save and sleep
- Leave the home screen alone: the pet moves about every 15 seconds
- After two quiet minutes: the pet becomes drowsy
- After three quiet minutes: the pet saves and falls asleep
- Press power to wake

Pet state is stored in the ESP32's NVS flash. Normal interaction and pet
movement use fast black-and-white refreshes. Every 12 visible updates, and on
boot, the firmware uses a full refresh to limit ghosting. Movement is step
animation rather than video: the pet changes pose and location every several
seconds, then uses low-power light sleep between 15-minute dream moments. After
two dream moments, the pet wakes itself and resumes its visible life.

## Install a release

The easiest first installation is the
[Book Pet web installer](https://unipheas.github.io/book-pet-x3/). Open it on
a desktop or laptop in a browser that supports Web Serial, connect and wake the
X3 with its magnetic USB cable, then choose **Install Book Pet**. No development
tools are required.

The USB installer cannot run on iPhone or iPad. After Book Pet is installed,
however, a phone can install future updates without an app or USB cable.

You can also download the files attached to the
[latest GitHub release](https://github.com/unipheas/book-pet-x3/releases/latest).

The release includes:

- `book-pet-x3-factory.bin` — merged image for a complete first-time flash
- `book-pet-x3-update.bin` — signed file for phone and SD updates
- `book-pet-x3.bin` — raw application image for developer workflows
- Bootloader and partition images
- SHA-256 checksums

See [Updating and recovery](docs/UPDATING.md) for every supported path.

## Update an installed Book Pet

Open **Pet Menu → Updates** on the X3.

- **Phone / Browser:** Book Pet shows a temporary Wi-Fi name and random
  password. Join it from a phone, open `192.168.4.1`, then upload the signed
  release file or ask the X3 to fetch the latest official version.
- **Update from SD:** copy `book-pet-x3-update.bin` to
  `/BOOKPET/UPDATE.BIN` on a FAT32 or exFAT card, insert it, then choose the SD
  option.
- **Restore previous:** return to the other known-good firmware slot. Pet
  progress is stored separately and remains intact.

Book Pet writes the inactive firmware slot first. It only switches after the
complete file, SHA-256 digest, and official release signature have passed
verification. Do not remove power while an installation is in progress.

## Build from source

### What you need

- XTEINK X3 **Developer Edition / overseas unlocked edition**
- The X3 magnetic pogo-pin USB cable (the X3 does not have USB-C)
- macOS, Windows, or Linux computer
- VS Code with the PlatformIO extension, or PlatformIO Core
- Git

Charge the X3 first. Back up books or files you care about before flashing.
Installing third-party firmware replaces the current application and may affect
the warranty. Do not flash an X4 or a USB-locked/restricted-market device.

### First-time setup

1. Install VS Code and the PlatformIO IDE extension.
2. Clone this repository with its pinned SDK:

   ```sh
   git clone --recurse-submodules https://github.com/unipheas/book-pet-x3.git
   cd book-pet-x3
   ```

   If you received this folder or zip instead:

   ```sh
   git submodule update --init --recursive
   ```

3. Open the folder in VS Code. PlatformIO downloads the compiler and board
   packages automatically on the first build.
4. Build with PlatformIO's **Build** button, or:

   ```sh
   pio run
   ```

### Flashing

1. Charge the X3 and keep it connected to the magnetic pogo-pin USB cable.
2. Wake/unlock the device.
3. Close serial monitors or other flashers.
4. Upload with PlatformIO's **Upload** button, or:

   ```sh
   pio run --target upload
   ```

5. If automatic port selection fails, list ports with `pio device list`, then
   add the detected port to `platformio.ini`, for example:

   ```ini
   upload_port = /dev/cu.usbmodemXXXX
   ```

Do not disconnect power during erase/write. The first boot performs a full
e-paper refresh and creates a fresh pet.

## Prebuilt binary

The `firmware/` folder contains the application image plus the bootloader,
partition table, and OTA boot helper produced/selected by the verified
PlatformIO build. PlatformIO writes them at the correct offsets; using `pio run
--target upload` is safer than manually choosing offsets.

The binary has been compiler-verified and tested on an XTEINK X3 Developer
Edition. The portrait orientation was verified on-device. The underlying
six-button input routes, persistent state, power-off, and power-button wake
were verified during the v0.1 hardware bring-up.

## Recovery and safety

Hold the front **Back** button while powering on to open **Recovery & Updates**
without starting normal pet activity. From there you can install from SD, start
the phone portal, or restore the previous firmware slot.

Keep a known-good CrossPoint or stock recovery image before the first install.
If neither Book Pet firmware slot boots, use the USB
[Book Pet web installer](https://unipheas.github.io/book-pet-x3/) for a clean
reinstall. Restricted devices can require a different recovery path, so confirm
that the computer sees the X3 as a serial device before erasing anything.

## Design notes

- Target: ESP32-C3, 16 MB flash, 792×528 physical X3 e-paper panel.
- UI: counter-clockwise rotation into a 528×792 logical portrait canvas.
- Hardware layer: pinned FreeInk SDK display, input, board-profile, panel
  detection, and power-management libraries.
- State: versioned `PetState` blob in ESP32 Preferences/NVS.
- Power: manual Rest releases the X3 power latch and remains off until the
  power button wakes it. FreeInk selects the power-button wake source from the
  active board profile; Book Pet retains its hardware-tested X3 GPIO 13 latch
  release until that latch is described upstream. Natural naps retain the latch
  and use timer-driven light sleep so the pet can dream and wake itself.
- Time: active minutes exclude autonomous naps. Calendar time and RTC-based
  aging are not implemented.

## Project layout

```text
src/             Pet state, controls, rendering, and firmware entry point
firmware/        Verified release binaries and checksums
freeink-sdk/     Pinned multi-device e-paper SDK Git submodule
site/            One-click browser installer
scripts/         Signed release packaging
platformio.ini   Reproducible ESP32-C3 build configuration
```

## Hardware test record

- Page controls register and route correctly: verified.
- Portrait orientation and screen margins: verified.
- Feed, Play, and power-button Rest: verified on the v0.1 hardware base.
- Power hold sleeps and power wakes: verified.
- Wake visibly leaves the Dreaming state: verified.
- Autonomous wandering, drowsiness, sleep, dreams, and self-wake: verified on
  X3 hardware for v0.4.
- FreeInk migration: portrait rendering, autonomous movement, repeated button
  input, manual Rest/wake, and natural sleep/light-sleep wake verified on an
  original-controller X3.
- The 15-minute timer-driven dream/self-wake cycle remains to be rechecked on
  FreeInk; it was verified on the preceding v0.4 hardware layer.
- Longer-term ghosting, charging-cycle persistence, and battery-life testing
  remain to be measured during normal use.

## Contributing

Bug reports, ideas, artwork, documentation, and code are welcome. Read
[CONTRIBUTING.md](CONTRIBUTING.md) for setup and pull-request guidance. Please
follow our [Code of Conduct](CODE_OF_CONDUCT.md), and use
[SECURITY.md](SECURITY.md) for security-sensitive reports.

## License

Book Pet is available under the [MIT License](LICENSE). You may use, copy,
modify, merge, publish, distribute, sublicense, and sell copies, provided the
license notice is preserved.

The pinned FreeInk SDK is also MIT-licensed; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## AI-assisted development

Book Pet has been developed extensively with OpenAI Codex as a coding,
research, documentation, and release collaborator under human direction.
No AI model runs in the released firmware: the pets' apparent thoughts and
personalities come from deterministic offline rules, and no pet data is sent
to an AI provider.

Read [AI_ASSISTED DEVELOPMENT DISCLOSURE](AI_DISCLOSURE.md) for the full
breakdown of AI involvement, human oversight, validation, and contribution
expectations.

Book Pet is an independent community project and is not affiliated with or
endorsed by XTEINK, FreeInk, CrossPoint, or OpenX4.
