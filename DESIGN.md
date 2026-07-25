# Design System — Book Pet

## Product context

- **What this is:** A standalone portrait virtual pet for the XTEINK X3
  Developer Edition.
- **Who it is for:** People who want a calm pocket companion and makers who
  enjoy understandable embedded projects.
- **Project type:** One-bit e-paper game and persistent ambient object.

## Aesthetic direction

- **Direction:** Pocket Pixel Familiar
- **Decoration:** Intentional—chunky pixel furniture, a room border, and small
  expressive marks without visual noise.
- **Mood:** A late-1990s handheld pet that discovered a quiet library. Cute,
  legible, and slightly strange.
- **Core rule:** The pet is a creature, never a logo, book, dashboard mascot,
  or generic rounded blob.

## Display and typography

- **Canvas:** 528×792 logical portrait pixels, rotated counter-clockwise onto the X3's 792×528
  physical panel.
- **Palette:** Black and white only. Pattern and negative space replace color.
- **Display type:** Custom 5×7 pixel face at integer scales.
- **Scale:** 1× metadata, 2× labels, 3× headings, 6–8× creature pixels.
- **Contrast:** Every primary label must remain readable at arm's length.

## Layout

- **Top strip (0–64):** Name, level, XP, Page Bites, and food inventory.
- **Pet room (72–420):** One dominant creature, thought bubble, and a grounded
  environment.
- **Needs (438–600):** Four compact meters: Full, Joy, Rest, Clean.
- **Action dock (612–680):** Feed, Play, Clean, Rest with one strong selection state.
- **Controls (736–792):** Persistent button hints.
- **Menus:** Full portrait screens, not tiny overlays.

## Controls

- **Front Left/Right:** Move horizontally or change action.
- **Front Confirm:** Select or perform.
- **Front Back:** Open menu or return home.
- **Side Up/Down:** Move vertically through menus.
- **Power hold:** Save and sleep.

## Pet personality

- **Species:** Byte, an original pixel creature.
- **Expression:** Pose, ears, eyes, mouth, and two or three emphasis marks.
- **Thoughts:** Short deterministic lines driven by needs, progress, recent
  actions, and personality. They must never claim to be sentient or online.
- **Economy:** Play and time uncover Page Bites. Page Bites can be baked into
  food. Page Catch awards Story, Mystery, Science, and Adventure fragments.
  Care and play award XP; levels change Byte's growth stage.
- **Memory:** A three-entry diary records the latest meaningful care and play
  moments rather than logging every simulation tick.
- **Personality:** Curious, Cozy, or Bold emerges from the balance of play,
  feeding, cleaning, and rest. It changes thoughts without claiming sentience.

## E-paper motion

- No idle refresh loop.
- Change pose only after input, a meaningful simulation tick, or wake.
- Use fast black-and-white refreshes for normal interaction.
- Force a cleanup refresh after twelve fast updates and on boot.

## Decisions log

| Date | Decision | Rationale |
|---|---|---|
| 2026-07-25 | Portrait-first UI | Matches how the X3 is held and exposes all six controls naturally. |
| 2026-07-25 | Original 8-bit creature | Creates a real pet identity without copying Tamagotchi artwork. |
| 2026-07-25 | Offline thought engine | Personality without accounts, radios, cloud cost, or deceptive AI claims. |
| 2026-07-25 | Page Bite economy | Gives the pet a book-world metaphor without making the pet itself a book. |
| 2026-07-25 | Memory-game page hunting | Makes earning pages active, tactile, and feasible on slow e-paper. |
| 2026-07-25 | Care-shaped personality | Creates apparent inner life using transparent offline rules. |
