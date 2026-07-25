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

## AI transparency

Keep `AI_DISCLOSURE.md` accurate when AI tools materially assist development,
documentation, artwork, testing, flashing, or releases. Never imply that Byte's
deterministic offline behavior is a hosted or on-device AI model.
