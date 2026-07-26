# Book Pet contributor notes

## Design system

Always read `DESIGN.md` before making visual or interaction decisions. All
orientation, hierarchy, pixel-art, control, economy, and e-paper refresh rules
are defined there. Do not deviate without an explicit design decision.

When reviewing UI changes, flag anything that:

- assumes a landscape logical canvas;
- turns the creature back into a book or logo;
- refreshes the panel rapidly or without a meaningful pose or movement change;
- hides essential actions behind network access;
- claims the offline thought engine is sentient or cloud AI.

## Hardware layer

Book Pet uses the pinned `freeink-sdk` submodule for the display, input, board
profile, X3 panel detection, and deep-sleep wake source. Keep the X3 GPIO 13
soft-power latch in Book Pet until FreeInk represents that latch in its X3
board profile and the replacement has been physically verified.

## Reader and persistence

Read `ARCHITECTURE.md` before changing the reader, storage, or reward path.
Keep user EPUBs and rebuildable caches on SD, per-book progress in SPIFFS, and
pet state plus the pending reward journal in NVS.

Book identity must remain content-derived rather than filename-derived.
Reading rewards must remain replayable and idempotent across interruption.
Hidden filesystem metadata must never be treated as a visible EPUB. Do not
let one invalid EPUB poison valid library entries. Per-book progress must keep
its checksum and backup recovery, and a previously initialized SPIFFS
partition must fail closed rather than auto-format after a mount failure. Do
not increase reader memory budgets without checking both firmware variants
and complex books on hardware.

## Testing

Follow `TESTING.md`. At minimum, run:

```sh
python3 scripts/validate_project.py
freeink-sdk/libs/book/FreeInkBook/test/host/run.sh
pio run -e xteink_x3
```

Build `xteink_x3_release` for any release, persistence, partition, signing,
update, or recovery change. A bug fix needs a regression assertion or fixture,
and a user-visible hardware change needs a clearly recorded X3 check.

## Firmware updates

Never change `partitions.csv`, update signature enforcement, the embedded public
key, HTTPS trust material, OTA boot confirmation, or rollback behavior without
running `python scripts/validate_project.py`, building both PlatformIO
environments, and physically testing the release build on an X3.

Official release builds must fail closed on missing or invalid signatures.
Developer builds may accept unsigned local firmware. Never commit, print,
attach, or log the private OTA signing key. Home Wi-Fi credentials must remain
RAM-only, and normal pet play must not enable the radio.

Preserve pet NVS through OTA, rollback, and recovery. A first-time factory
installer may offer a full erase because an unknown preexisting partition table
cannot be preserved safely. Preserve the reading-progress SPIFFS partition
during normal OTA and rollback as well.

## AI transparency

Keep `AI_DISCLOSURE.md` accurate when AI tools materially assist development,
documentation, artwork, testing, flashing, or releases. Never imply that Byte's
deterministic offline behavior is a hosted or on-device AI model.
