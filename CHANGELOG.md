# Changelog

All notable changes to Book Pet are documented here.

## [Unreleased]

### Changed

- Migrated the X3 hardware layer from the legacy community SDK to a pinned
  FreeInk SDK submodule.
- Moved display pins, button wake polarity, and deep-sleep wake setup to
  FreeInk board and power abstractions.
- Added startup detection for both original and newer X3 display-controller
  revisions while preserving the existing portrait UI and pet save data.

## [0.4.0] - 2026-07-25

### Added

- Meet Mote after finishing one book and Pip after finishing three.
- Log real reading progress and earn one food for every ten lifetime pages.
- Finish books to discover Page Ball, Tiny Bell, Letter Blocks, and Paper Kite.
- Equip unlocked toys and see them in the pet's room.
- Use the dedicated Pet Life screen to control autonomous behavior.
- Watch the active pet wander, investigate, play, become drowsy, fall asleep,
  dream, and wake itself through battery-conscious step animation.
- Preserve v0.2, v0.3, and development-v4 progress through automatic v5
  save-data migration.

### Changed

- Reading rewards now have a focused menu separate from pet animation settings.
- Diary, stats, Page Catch, and the home room now follow the selected pet.
- Manual Rest remains asleep until power-button wake, while natural naps use
  sparse timer-driven dream moments.

## [0.3.0] - 2026-07-25

### Added

- Care for Byte with a fourth Clean need and action.
- Play the three-lane Page Catch memory game to earn Page Bites and XP.
- Collect Story, Mystery, Science, and Adventure page fragments.
- Watch Byte develop a Curious, Cozy, or Bold personality from care choices.
- Grow Byte through Hatchling, Sprout, and Familiar life stages.
- Read Byte's three-entry diary of recent care, play, and growth moments.
- See new Hungry, Tired, and Dirty pixel-art poses.
- Preserve v0.2 progress through an automatic v3 save-data migration.

### Changed

- Page Bites now come from active Page Catch play instead of passive time.
- Need decay now accumulates correctly across short sessions and only refreshes
  the e-paper display when a visible value changes.
- The home screen now fits four needs and four actions in portrait mode.

## [0.2.0] - 2026-07-25

### Added

- A portrait 528×792 interface for the X3's phone-back orientation.
- Byte, an original 8-bit pixel familiar with idle, happy, and sleeping poses.
- Page Bites, food inventory, a pantry, experience, levels, and level rewards.
- Mood- and event-aware on-device thoughts without a network or AI service.
- Menu screens for the pantry, lifetime stats, and future selectable pets.
- Dedicated navigation across all six physical page buttons.
- Pure compile-time checks for progression and resource rules.

### Changed

- Replaced the original book-shaped character and landscape layout.
- Renamed Sleep to Rest while preserving power-button sleep behavior.
- Advanced persisted pet data to version 2 for the new progression fields.

## [0.1.0] - 2026-07-25

### Added

- A standalone book-shaped virtual pet for the XTEINK X3 Developer Edition.
- Feed, Play, and Sleep actions with fullness, joy, and rest meters.
- Persistent local state with no network, phone, account, or cloud dependency.
- E-paper-aware fast refreshes with periodic full cleanup refreshes.
- Support for the X3's page controls across both input ladders.
- Automatic sleep and a visible power-button wake transition.
- Hardware-tested firmware images and reproducible PlatformIO configuration.

[0.1.0]: https://github.com/unipheas/book-pet-x3/releases/tag/v0.1.0
[0.2.0]: https://github.com/unipheas/book-pet-x3/releases/tag/v0.2.0
[0.3.0]: https://github.com/unipheas/book-pet-x3/releases/tag/v0.3.0
[0.4.0]: https://github.com/unipheas/book-pet-x3/releases/tag/v0.4.0
