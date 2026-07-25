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
- **Pet Life:** Autonomous behavior has its own screen. Reading contains only
  reading actions and rewards.

## Controls

- **Front Left/Right:** Move horizontally or change action.
- **Front Confirm:** Select or perform.
- **Front Back:** Open menu or return home.
- **Side Up/Down:** Move vertically through menus.
- **Power hold:** Save and sleep.

## Pet personality

- **Species:** Byte, Mote, and Pip—three original pixel creatures unlocked
  through reading progress.
- **Expression:** Pose, ears, eyes, mouth, and two or three emphasis marks.
- **Thoughts:** Short deterministic lines driven by needs, progress, recent
  actions, and personality. They must never claim to be sentient or online.
- **Economy:** Page Catch awards Page Bites plus Story, Mystery, Science, and
  Adventure fragments. Page Bites can be baked into food. Logged pages earn
  food directly, and finished books unlock toys and pets. Care, play, and
  reading award XP; levels change the current pet's growth stage.
- **Memory:** A three-entry diary records the latest meaningful care and play
  moments rather than logging every simulation tick.
- **Personality:** Curious, Cozy, or Bold emerges from the balance of play,
  feeding, cleaning, and rest. It changes thoughts without claiming sentience.
- **Reading:** People log real pages after reading. Each ten lifetime pages
  earns one food; finishing a book discovers a toy and can unlock a new pet.
- **Pet family:** Byte is joined by Mote and Pip, each with an original
  one-bit silhouette and a book-reading unlock path.

## E-paper life and motion

- Treat the home screen as a living diorama, not a static status page.
- While awake and unattended, the pet changes expression, wanders between
  grounded positions, investigates the room, and reacts to its equipped toy.
- Movement is readable step animation rather than fake smooth motion: one
  meaningful pose every several seconds, never rapid full-screen animation.
- The pet visibly becomes drowsy before naturally falling asleep.
- During low-power sleep, sparse timer wakes advance a dream pose with one
  refresh. After resting, the pet may wake itself and resume its visible life.
- Manual Rest and a power-button sleep remain quiet until the person wakes the
  device; autonomous behavior never takes away that explicit control.
- Use fast black-and-white refreshes for normal interaction and awake moments.
  Force a cleanup refresh after twelve fast updates and on boot.
- The Pet Life screen explains and toggles autonomous behavior. Reading never
  contains animation or device-behavior settings.

## Decisions log

| Date | Decision | Rationale |
|---|---|---|
| 2026-07-25 | Portrait-first UI | Matches how the X3 is held and exposes all six controls naturally. |
| 2026-07-25 | Original 8-bit creature | Creates a real pet identity without copying Tamagotchi artwork. |
| 2026-07-25 | Offline thought engine | Personality without accounts, radios, cloud cost, or deceptive AI claims. |
| 2026-07-25 | Page Bite economy | Gives the pet a book-world metaphor without making the pet itself a book. |
| 2026-07-25 | Memory-game page hunting | Makes earning pages active, tactile, and feasible on slow e-paper. |
| 2026-07-25 | Care-shaped personality | Creates apparent inner life using transparent offline rules. |
| 2026-07-25 | Reading earns care resources | Connects the virtual-pet economy to real-world reading without a phone or account. |
| 2026-07-25 | Step-based ambient moments | Makes the retained e-paper scene feel alive while bounding battery use and ghosting. |
| 2026-07-25 | Living-diorama rhythm | Visible wandering, drowsiness, dreams, and self-waking feel alive while remaining honest about step-based e-paper motion. |
| 2026-07-25 | Separate Pet Life screen | Keeps reading rewards focused and makes autonomous behavior understandable and discoverable. |
