# Reading books

Book Pet is an offline EPUB reader as well as a virtual pet. Books, page caches,
and reading happen on the X3; no account, phone, Wi-Fi connection, or cloud
service is required.

## Add books

1. Format a microSD card as FAT32 or exFAT.
2. Create `/BOOKS` at the top of the card.
3. Copy DRM-free files ending in `.epub` directly into that folder.
4. Insert the card before opening the Library.

The library reads up to 24 top-level EPUB files and sorts them by filename.
Subfolders are not scanned. Hidden files are ignored, including macOS
AppleDouble metadata such as `._Book.epub`.

Example:

```text
SD card
├── BOOKS
│   ├── Alice in Wonderland.epub
│   ├── The Secret Garden.epub
│   └── Winnie-the-Pooh.epub
└── BOOKPET
    └── CACHE
```

Book Pet creates `/BOOKPET/CACHE` automatically. That folder contains
rebuildable FreeInkBook indexes and page-layout caches. Do not store personal
files there. Deleting the cache is safe; the next open rebuilds it, while the
resume position and pet rewards remain in the X3's internal reading-progress
partition.

## Use the Books menu

- **Continue Reading** reopens the last book whose current-book journal was
  saved and which is still present on the SD card.
- **Library** rescans `/BOOKS` and lists compatible filenames.
- **Reading Rewards** explains lifetime pages, Page Bites, food, finished
  books, and unlock milestones.

Removing a book from the card does not immediately delete its internal progress
record. Putting the same unchanged EPUB back later restores its position.
Replacing it with a different edition creates a different content identity and
therefore a separate record.

## Reader controls

- Side Up or Front Left: previous page
- Side Down, Front Right, or Front Confirm: next page
- Front Back: save the current position and return to Library
- Hold Power: save the current position and put Book Pet to sleep

The first open of a book or chapter can take a moment while the X3 indexes and
paginates it. Later page turns read the cached layout from the SD card.

The reader is intentionally e-paper native. A page turn is a complete,
meaningful refresh rather than smooth scrolling. A full cleanup refresh is used
periodically to limit ghosting.

## Progress and rewards

Book Pet records stable EPUB locations instead of trusting a manual page count.
A forward page can award its reading reward once:

- each new page: 1 Page Bite and 1 XP
- each 10 lifetime pages: 1 food
- first completion of a book: 5 Page Bites, 25 XP, and the next toy
- first finished book: unlock Mote
- third finished book: unlock Pip

Reading backwards, reopening a book, refreshing the display, or reinstalling
the same firmware does not duplicate rewards. Every opened book keeps its own
resume position, forward-reading high-water mark, and completion record.
Renaming the same EPUB preserves that record because Book Pet identifies the
book from its ZIP contents rather than its filename.

Progress is saved when you turn a page, return to the Library, hold Power, or
reach the reader's inactivity sleep. A small replay journal coordinates the
book record and pet save: if power is interrupted between those writes, Book
Pet can finish the pending transaction without granting the same reward twice.

Page counts are Book Pet's laid-out e-paper pages, not the print-page number
shown by a publisher. Changing fonts or layout code in a future release may
change visible page boundaries, but the stable chapter and character anchors
keep resume and reward high-water marks meaningful.

## EPUB support

The reader uses FreeInkBook's ESP32-C3 small-memory profile. It supports EPUB 2
and EPUB 3 package metadata, reading order, common XHTML/CSS text layout, PNG
and JPEG images, Unicode line breaking, and SD-backed caching.

The following cannot be read:

- EPUBs protected by DRM
- malformed or incomplete EPUB archives
- books whose chapter/catalog memory needs exceed the X3's available RAM
- unsupported image types such as GIF, SVG, or interlaced PNG; their reserved
  space may appear blank while the surrounding text remains readable

Complex typography is simplified for a small monochrome screen. Scripts,
video, audio, interactive widgets, external web fonts, and publisher-specific
reader extensions are not executed.

## Storage and privacy

| Data | Stored in | Purpose |
|---|---|---|
| EPUB files | SD `/BOOKS` | User-owned library |
| Catalog and page cache | SD `/BOOKPET/CACHE` | Rebuildable reader speed-up |
| Resume and reward high-water mark | Internal SPIFFS | Durable per-book progress |
| Current book and pending reward | Internal NVS | Continue Reading and crash-safe coordination |
| Pet rewards and inventory | Internal NVS | Pet progression |

Reading never enables Wi-Fi and no reading history leaves the device. A normal
firmware update preserves NVS and SPIFFS. A clean factory install that erases
internal flash resets both pet and reading progress, while books on the
removable SD card remain separate.

## Troubleshooting

**Library Not Ready: SD card not found**

Confirm the card is inserted fully and formatted as FAT32 or exFAT. Power the X3
off, reinsert the card, wake it, and check Library again.

**No EPUB books yet**

The folder must be exactly `/BOOKS`, not `/BOOKPET/BOOKS`, and files must end in
`.epub`. Put files directly in that folder rather than a subfolder.

**Library Not Ready: no readable EPUB files were found**

Every visible `.epub` file was incomplete or did not use a ZIP container that
Book Pet supports. Open the files in another reader, then remove or recopy the
failing files. If at least one book is readable, Book Pet lists it and skips bad
entries rather than blocking the entire library. Book Pet v1.0 and later also
ignores hidden macOS `._` files automatically.

**Book could not open**

Try the EPUB in another reader first. Store-bought books may be DRM protected
even though the filename ends in `.epub`. Book Pet automatically rebuilds a
damaged index once. If the book is valid and DRM-free but still does not open,
delete `/BOOKPET/CACHE` and let Book Pet recreate the caches.

**Chapter is too complex for X3**

The ESP32-C3 has limited RAM. Try a simpler edition of the book. Book Pet fails
cleanly rather than risking the pet save or SD contents.

**Progress Not Saved**

The internal reading-progress filesystem could not complete a durable write,
or its checksum found a damaged record. Restart Book Pet and try the page
again. The reward journal fails closed: it does not intentionally advance the
rewarded page until the save can be completed. Normal startup never formats a
previously initialized progress partition after a mount failure.

**The first open is slow**

This is normal for a large first chapter or a book with many chapters. Keep the
SD card inserted and wait for indexing. Later opens reuse the cache.

**The card works on a computer but not the X3**

Safely eject it from the computer, reinsert it fully, and retry. FAT32 is the
most widely compatible choice. Avoid removing the card while Book Pet is
indexing, turning a page, or writing progress-related cache data.

## Reader test record

The v1.0 hardware check used a FAT32 card prepared on macOS. The X3:

- detected the card and `/BOOKS`;
- ignored Finder's hidden `._Book-Pet-Reader-Test.epub` metadata file;
- listed and opened the real EPUB;
- indexed and rendered the book;
- turned a page;
- returned to the Library; and
- reopened at the saved position without a progress-storage error.
