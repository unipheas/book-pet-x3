# Updating and recovery

Book Pet supports four installation and recovery paths. Pet progress lives in
the NVS data partition, separate from both firmware slots. Normal updates and
rollbacks do not erase it.

## First install: web installer

Use this for a stock X3, a complete reinstall, or recovery when neither firmware
slot starts.

1. Confirm the device is an unlocked XTEINK X3 Developer/overseas edition.
2. Back up books or other files you care about.
3. Open <https://unipheas.github.io/book-pet-x3/> on a desktop or laptop in a
   browser with Web Serial support.
4. Connect the X3 magnetic pogo-pin USB cable and wake the device.
5. Select **Install Book Pet**, choose the X3 serial device, and follow the
   clean-install prompt.
6. Keep the cable connected until the X3 restarts and completes its first full
   e-paper refresh.

The first install writes one merged ESP32-C3 image at flash offset `0x0`.
It contains the bootloader, OTA partition table, OTA selector, and application.
The browser offers the erase choice because replacing unrelated firmware while
preserving an unknown partition layout is unsafe.

iOS browsers do not expose the USB Web Serial connection. Use a desktop or
laptop for the first install.

## Phone or local browser update

This needs no app, account, or USB cable.

1. Download `book-pet-x3-update.bin` from the latest GitHub release, or plan to
   let the X3 retrieve the official release itself.
2. On the X3, open **Pet Menu → Updates → Phone / Browser**.
3. Join the `BookPet-XXXXXX` Wi-Fi network using the random password shown on
   the X3.
4. Open <http://192.168.4.1/> if the maintenance page does not appear
   automatically.
5. Either:
   - choose the signed `.bin` and select **Verify and install**, or
   - enter home Wi-Fi details and select **Check and install**.
6. Keep the X3 powered until it verifies the firmware and restarts.

The X3 creates a private WPA access point for the session. A home Wi-Fi password
is kept in RAM only, is not written to the pet save, and is cleared after the
connection attempt. The local upload does not require internet access.

## SD card update

1. Format a microSD card as FAT32 or exFAT.
2. Create a folder named `BOOKPET`.
3. Copy the official `book-pet-x3-update.bin` into that folder and rename it
   `UPDATE.BIN`.
4. Optionally copy a SHA-256 sidecar to `UPDATE.BIN.sha256` or
   `UPDATE.SHA256`.
5. Insert the card and open **Pet Menu → Updates → Update from SD**.
6. Keep the X3 powered until it verifies the firmware and restarts.

For convenience, Book Pet also recognizes:

- `/BOOKPET/book-pet-x3-update.bin`
- `/book-pet-x3-update.bin`
- `/update.bin`

Official release firmware rejects an unsigned or incorrectly signed image even
when no sidecar checksum is present.

## Restore the previous firmware

Open **Pet Menu → Updates → Restore previous** and confirm a second time.
Book Pet selects the other bootable OTA slot and restarts. This option appears
as unavailable until at least one OTA update has populated both slots.

Rollback changes firmware only. It does not rewind pet progress or erase NVS.
New firmware must keep persisted pet-state migrations backward compatible.

## Hold-at-boot recovery

Hold the front **Back** button while powering on. Keep holding it through the
first moment of boot. Book Pet opens **Recovery & Updates** instead of the home
screen. SD update, phone update, rollback, and About remain available.

If the device cannot reach recovery, perform a clean USB web install.

## Update safety model

- Two 6.5 MiB application slots: the running slot is never overwritten.
- The new slot is selected only after the full stream finishes.
- Official firmware requires an RSA-4096/SHA-256 signature.
- Local uploads calculate and supply SHA-256 before installation.
- Official online updates require HTTPS, a valid manifest, an allowed URL,
  expected size, SHA-256, and a newer semantic version.
- A newly booted slot is confirmed only after board selection, pet-state load,
  display initialization, and a complete first render.
- If an unconfirmed image crashes during boot, the ESP32 bootloader can return
  to the previous valid slot.

Do not disconnect power while the screen says an update is installing.
