# Design System — Book Pet

## Product context

- **What this is:** A standalone portrait virtual pet and offline EPUB reader
  for the XTEINK X3 Developer Edition.
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
- **Main navigation:** Two large doors only—**My Pet** and **Books**.
- **Pet Nook:** Pantry, Toy Box, and Diary.
- **Pet Settings:** Pet Life, Stats, Choose Pet, and Updates.
- **Books:** Continue Reading, Library, and Reading Rewards.
- **Menus:** Full portrait screens, not tiny overlays. Every submenu shows
  where Back returns.
- **Pet Life:** Autonomous behavior has its own screen. Reading contains only
  reading actions and rewards.
- **Updates:** A plain-language maintenance screen keeps install methods,
  recovery, version details, and rollback out of normal pet play. Destructive
  recovery actions always require a second confirmation.

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
- **Economy:** A newly completed reader page awards one Page Bite and one XP.
  Every ten lifetime pages awards one food. Page Bites can be baked into food,
  and a book's first completion unlocks a toy and can unlock a pet. Page Catch
  is play—it awards joy and a small amount of XP, never reading currency.
- **Memory:** A three-entry diary records the latest meaningful care and play
  moments rather than logging every simulation tick.
- **Personality:** Curious, Cozy, or Bold emerges from the balance of play,
  feeding, cleaning, and rest. It changes thoughts without claiming sentience.
- **Reading:** FreeInkBook opens EPUB files from `/BOOKS` on the SD card,
  streams and paginates chapters, and caches layout on the card. Stable
  `(spine, charStart)` locations preserve resume position and ensure that
  revisiting a page cannot award its reward again. The first completion of a
  book discovers a toy and can unlock a new pet. A content-derived 64-bit book
  identity keeps progress across renames and isolates replaced files. A
  replayable reward transaction keeps the book ledger and pet state consistent
  across an interrupted save. Per-book records live as atomic root-level files
  in the X3's dedicated internal filesystem rather than consuming one NVS key
  per book.
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
| 2026-07-25 | Page Catch as pet play | Adds an active tactile game that is feasible on slow e-paper without pretending game actions are real reading. |
| 2026-07-25 | Care-shaped personality | Creates apparent inner life using transparent offline rules. |
| 2026-07-25 | Reading earns care resources | Connects the virtual-pet economy to real-world reading without a phone or account. |
| 2026-07-25 | Step-based ambient moments | Makes the retained e-paper scene feel alive while bounding battery use and ghosting. |
| 2026-07-25 | Living-diorama rhythm | Visible wandering, drowsiness, dreams, and self-waking feel alive while remaining honest about step-based e-paper motion. |
| 2026-07-25 | Separate Pet Life screen | Keeps reading rewards focused and makes autonomous behavior understandable and discoverable. |
| 2026-07-25 | FreeInk hardware layer | Centralizes board profiles and X3 panel detection while leaving the pet engine and portrait UI independent of the hardware SDK. |
| 2026-07-25 | Offline-first optional updates | Radios remain off during play; a deliberate maintenance screen offers SD, temporary local Wi-Fi, and signed online updates. |
| 2026-07-25 | Dual-slot signed recovery | Inactive-slot writes, release signatures, delayed boot confirmation, and previous-slot rollback make updating approachable without treating failure as unrecoverable. |
| 2026-07-26 | Two-door information architecture | My Pet and Books communicate the product immediately; Pet Nook and Pet Settings keep secondary features understandable. |
| 2026-07-26 | Reader-verified progression | EPUB page anchors replace manual page logging so reading progress, XP, food, and completion rewards are earned once and resume safely. |
| 2026-07-26 | Retire fragments | A single Page Bite economy is easier to understand; legacy fragments migrate into Page Bites. |
| 2026-07-26 | Content-based book identity | Renames preserve progress while replaced EPUBs cannot reuse old page caches or rewards. |
| 2026-07-26 | Replayable reading rewards | A pending transaction is recorded before pet state changes so interrupted saves can finish without loss or duplication. |
