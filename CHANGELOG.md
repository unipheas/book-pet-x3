# Changelog

All notable changes to Book Pet are documented here.

## [Unreleased]

## [1.0.0] - 2026-07-26

### Added

- Read DRM-free EPUB books directly on the X3 with a complete offline library
  and portrait reader powered by FreeInkBook.
- Resume every book where you left off, with SD-backed indexing, chapter
  pagination, layout caching, image support, and clear recovery messages.
- Keep durable progress for each book using content-based identity, so renaming
  an EPUB preserves its place while replacing the file starts a separate record.
- Earn each page reward once through an interruption-safe journal that keeps
  reading progress and pet rewards consistent across restarts.

### Changed

- Rebuilt navigation around two primary sections: **My Pet** and **Books**.
- Grouped Pantry, Toy Box, and Diary under **Pet Nook**.
- Grouped Pet Life, Stats, Choose Pet, and Updates under **Pet Settings**.
- Replaced the manual reading log with Continue Reading, Library, and Reading
  Rewards.
- Reading now drives progression: each new page earns one Page Bite and one XP,
  every ten pages earns food, and a book's first completion unlocks rewards.
- Page Catch now awards joy and play XP instead of reading currency.
- Legacy Story, Mystery, Science, and Adventure fragments migrate into Page
  Bites and no longer appear in the interface.

### Fixed

- Ignore hidden macOS AppleDouble files such as `._Book.epub` so copying books
  from a Mac cannot make a valid SD library report an invalid EPUB.
- Skip malformed or unsupported EPUB files individually so one bad file cannot
  block valid books or create an unsafe shared cache identity.
- Mount and, when necessary, initialize the dedicated reading-progress
  filesystem correctly after a new partition layout is installed, then fail
  closed instead of formatting it if a later mount fails.
- Checksum per-book progress records and recover from their backup copies,
  preventing damaged high-water marks from silently resetting rewards.
- Rebuild a damaged EPUB index once before asking the reader to clear its SD
  cache.
- Preserve the previous Continue Reading selection in memory when a progress
  save fails, allowing a later retry to repair the journal instead of silently
  losing the current-book pointer.
- Surface progress-save failures during book open, Back, page movement, manual
  sleep, and inactivity sleep instead of silently leaving an older position.
- Show a general **Library Not Ready** message for library and EPUB errors
  instead of incorrectly claiming every scan failure means the SD card is
  missing.

### Verified

- Built the normal X3 and signature-enforcing release firmware with FreeInkBook
  in its ESP32-C3 small-memory profile.
- Passed FreeInkBook's 36,755-check host suite, including 1,700-chapter EPUB
  catalog and small-memory pagination tests.
- On X3 hardware, scanned a FAT32 card prepared on macOS, ignored its hidden
  metadata file, opened the real EPUB, turned a page, returned to the library,
  and resumed at the saved location.

## [0.5.0] - 2026-07-25

### Added

- Added an on-device Updates screen with SD card, phone/browser, About, and
  previous-version restore options.
- Added a temporary WPA-protected local update portal that needs no phone app
  and never saves home Wi-Fi credentials.
- Added signed HTTPS updates from the official Book Pet release manifest.
- Added dual-slot OTA installation, deferred boot confirmation, and rollback to
  the previous working firmware.
- Added hold-Back-at-power-on recovery mode.
- Added a one-click ESP Web Tools installer and automated GitHub Pages update
  service.
- Added reproducible RSA-4096 signing, factory-image packaging, checksums, and
  tag-driven GitHub release automation.
- Added an approval-protected signing environment and isolated signing job so
  build dependencies never share a runner with the private release key.

### Changed

- Migrated the X3 hardware layer from the legacy community SDK to a pinned
  FreeInk SDK submodule.
- Moved display pins, button wake polarity, and deep-sleep wake setup to
  FreeInk board and power abstractions.
- Added startup detection for both original and newer X3 display-controller
  revisions while preserving the existing portrait UI and pet save data.
- Official release builds now reject local and SD firmware without the Book Pet
  release signature. Developer builds remain friendly to unsigned local work.
- New OTA slots now remain pending through a five-second healthy runtime window
  instead of being confirmed immediately after the first render.
- A power hold during that window now waits for the normal health deadline
  instead of confirming the new slot early.

### Fixed

- Fixed X3 SD-card detection after display startup by preserving the shared
  SPI bus's MISO connection across boot, wake, and display recovery.
- Replaced a broken framework signing hook with fail-closed Book Pet RSA
  verification that retains and checks the complete 512-byte release signature
  before activating an OTA slot.
- Recovery mode now consumes the power-on press before accepting new input, so
  the recovery screen no longer immediately returns to sleep.
- Reopening the phone updater no longer registers duplicate web routes, and
  status responses safely handle unusual Wi-Fi names.
- Phone updates now serialize all install operations, use a high-entropy access
  password plus per-session request token, and erase home Wi-Fi credentials
  after every online check.
- Release packaging now refuses unexpected files so local test artifacts cannot
  be swept into a published release.
- The USB installer now self-hosts the complete pinned ESP Web Tools module
  tree under a restrictive same-origin content-security policy.
- Release tags must point to commits already on `main`, and release packaging
  runs without persisted repository credentials or newly downloaded tools.
- Release builds install a hash-locked PlatformIO dependency set and verify the
  pinned pioarduino platform archive before compilation.

### Verified

- Installed a valid signed update from SD and through the local browser portal
  on X3 hardware.
- Rejected a deliberately corrupted RSA signature without rebooting or changing
  the running firmware.
- Restored the previous OTA slot without losing pet progress.
- Entered recovery by holding Back at power-on and confirmed it remains open.

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
[0.5.0]: https://github.com/unipheas/book-pet-x3/releases/tag/v0.5.0
[1.0.0]: https://github.com/unipheas/book-pet-x3/releases/tag/v1.0.0
[Unreleased]: https://github.com/unipheas/book-pet-x3/compare/v1.0.0...HEAD
