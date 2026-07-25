# Book Pet for XTEINK X3 Developer Edition

A small, completely standalone e-paper virtual pet. It has one book-shaped pet,
a status screen, feed/play/sleep actions, persistent local state, and a
conservative display policy.

This is an early hardware-targeted build. It is based on the X3 support in
CrossPoint Reader and its community SDK, pinned when the included binary was
built. It does not use Wi-Fi, Bluetooth, an account, or a phone.

## Controls

- Upper page button: choose **Feed**, **Play**, or **Sleep**
- Lower page button: perform the selected action
- Hold power for about 1.2 seconds: save and sleep
- Two minutes without input: save and sleep
- Press power to wake

Pet state is stored in the ESP32's NVS flash. Normal interaction uses fast
black-and-white refreshes. Every 12 visible updates, and on boot, the firmware
uses a full refresh to limit ghosting. It does not run cosmetic animation loops.

## What you need

- XTEINK X3 **Developer Edition / overseas unlocked edition**
- The X3 magnetic pogo-pin USB cable (the X3 does not have USB-C)
- macOS, Windows, or Linux computer
- VS Code with the PlatformIO extension, or PlatformIO Core
- Git

Charge the X3 first. Back up books or files you care about before flashing.
Installing third-party firmware replaces the current application and may affect
the warranty. Do not flash an X4 or a USB-locked/restricted-market device.

## First-time setup

1. Install VS Code and the PlatformIO IDE extension.
2. Clone this repository with its pinned SDK:

   ```sh
   git clone --recurse-submodules <your-book-pet-repository-url>
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

## Flashing

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

The binary has been compiler-verified for the current X3 target but has not been
run on the user's physical device yet. Treat the first flash as a hardware test.

## Recovery and safety

Keep a known-good CrossPoint or stock recovery image before flashing. If Book
Pet does not boot, use the CrossPoint web flasher/recovery process appropriate
for your exact X3 edition. Restricted devices can require a different recovery
path, so confirm that the computer sees the X3 as a serial device before erasing
anything.

## Design notes

- Target: ESP32-C3, 16 MB flash, 792×528 X3 e-paper panel.
- Base: CrossPoint community SDK display and input libraries.
- State: versioned `PetState` blob in ESP32 Preferences/NVS.
- Power: saves state, sleeps the panel, releases the X3 power latch, then enters
  deep sleep using CrossPoint's proven sequence.
- Time: version 0.1 tracks active minutes, not real-world time while powered
  off. RTC-based aging is intentionally deferred until the first on-device test.

## Next hardware test checklist

- Both page buttons register in the expected order.
- Power hold sleeps, and power wakes.
- Display orientation and margins are correct.
- No visible ghosting after 12–20 actions.
- State survives sleep and a charging cycle.
- Battery behavior is acceptable over a day of casual use.
