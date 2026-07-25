# Book Pet

[![Release](https://img.shields.io/github/v/release/unipheas/book-pet-x3)](https://github.com/unipheas/book-pet-x3/releases/latest)
[![Build](https://github.com/unipheas/book-pet-x3/actions/workflows/build.yml/badge.svg)](https://github.com/unipheas/book-pet-x3/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A tiny, standalone e-paper virtual pet for the **XTEINK X3 Developer
Edition**. Book Pet lives entirely on the device: no phone, Wi-Fi, Bluetooth,
account, cloud service, or AI connection required.

Feed it, play with it, put it to sleep, and carry it on the back of your phone.
Its state is saved locally, and its deliberately calm refresh behavior is
designed around the strengths of e-paper rather than pretending to be a
high-frame-rate screen.

> [!WARNING]
> Book Pet replaces the device firmware. It is intended only for the unlocked
> XTEINK X3 Developer/overseas edition. Do not flash it on an X4 or a
> USB-locked/restricted-market X3. Back up anything you care about first.

## Features

- One book-shaped monochrome pet
- Feed, Play, and Sleep actions
- Fullness, joy, and rest meters
- Persistent local state in ESP32 NVS
- Automatic sleep after two minutes of inactivity
- Physical page-button controls across X3 button layouts
- Conservative fast refreshes with periodic full cleanup refreshes
- Complete offline operation
- Reproducible PlatformIO build
- Hardware-tested release binaries

## Controls

- Any upper/left/back-style page button: choose **Feed**, **Play**, or **Sleep**
- Any lower/right/confirm-style page button: perform the selected action
- Hold power for about 1.2 seconds: save and sleep
- Two minutes without input: save and sleep
- Press power to wake

Pet state is stored in the ESP32's NVS flash. Normal interaction uses fast
black-and-white refreshes. Every 12 visible updates, and on boot, the firmware
uses a full refresh to limit ghosting. It does not run cosmetic animation loops.

## Install a release

The easiest path is to download the files attached to the
[latest GitHub release](https://github.com/unipheas/book-pet-x3/releases/latest).

The release includes:

- `book-pet-x3-factory.bin` — merged image for a complete first-time flash
- `book-pet-x3.bin` — application image for PlatformIO/developer workflows
- Bootloader and partition images
- SHA-256 checksums

The safest supported installation path is still PlatformIO, because it selects
the correct serial port and writes every image at the correct offset.

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
Edition. All page-button routes, Feed, Play, Sleep, persistent state, power-off,
and power-button wake were exercised on the physical device.

## Recovery and safety

Keep a known-good CrossPoint or stock recovery image before flashing. If Book
Pet does not boot, use the CrossPoint web flasher/recovery process appropriate
for your exact X3 edition. Restricted devices can require a different recovery
path, so confirm that the computer sees the X3 as a serial device before erasing
anything.

## Design notes

- Target: ESP32-C3, 16 MB flash, 792×528 X3 e-paper panel.
- Hardware layer: pinned CrossPoint community SDK display and input libraries.
- State: versioned `PetState` blob in ESP32 Preferences/NVS.
- Power: saves state, sleeps the panel, releases the X3 power latch, then enters
  deep sleep using CrossPoint's proven sequence.
- Time: version 0.1 tracks active minutes, not real-world time while powered
  off. RTC-based aging is not implemented.

## Project layout

```text
src/             Pet state, controls, rendering, and firmware entry point
firmware/        Verified release binaries and checksums
community-sdk/   Pinned display/input SDK Git submodule
platformio.ini   Reproducible ESP32-C3 build configuration
```

## Hardware test record

- Page controls register and route correctly: verified.
- Feed, Play, and Sleep: verified.
- Power hold sleeps and power wakes: verified.
- Wake visibly leaves the Dreaming state: verified.
- Display orientation and margins: verified.
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

The pinned community SDK is also MIT-licensed; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Book Pet is an independent community project and is not affiliated with or
endorsed by XTEINK or the CrossPoint project.
