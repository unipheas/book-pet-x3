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
cannot be preserved safely.

## AI transparency

Keep `AI_DISCLOSURE.md` accurate when AI tools materially assist development,
documentation, artwork, testing, flashing, or releases. Never imply that Byte's
deterministic offline behavior is a hosted or on-device AI model.
