# Reading books

Book Pet is an offline EPUB reader as well as a virtual pet. Books, page caches,
and reading happen on the X3; no account, phone, Wi-Fi connection, or cloud
service is required.

## Add books

1. Format a microSD card as FAT32 or exFAT.
2. Create `/BOOKS` at the top of the card.
3. Copy DRM-free files ending in `.epub` into that folder.
4. Insert the card before opening the Library.

Book Pet creates `/BOOKPET/CACHE` automatically. That folder contains
rebuildable FreeInkBook indexes and page-layout caches. Do not store personal
files there. Deleting the cache is safe; the next open rebuilds it, while the
resume position and pet rewards remain in the X3's own saved state.

## Reader controls

- Side Up or Front Left: previous page
- Side Down, Front Right, or Front Confirm: next page
- Front Back: save the current position and return to Library
- Hold Power: save the current position and put Book Pet to sleep

The first open of a book or chapter can take a moment while the X3 indexes and
paginates it. Later page turns read the cached layout from the SD card.

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
resume position, forward-reading high-water mark, and completion record in the
X3's saved state. Renaming the same EPUB preserves that record because Book Pet
identifies the book from its contents rather than its filename.

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

## Troubleshooting

**SD card needed**

Confirm the card is inserted fully and formatted as FAT32 or exFAT. Power the X3
off, reinsert the card, wake it, and check Library again.

**No EPUB books yet**

The folder must be exactly `/BOOKS`, not `/BOOKPET/BOOKS`, and files must end in
`.epub`.

**Book could not open**

Try the EPUB in another reader first. Store-bought books may be DRM protected
even though the filename ends in `.epub`.

**Chapter is too complex for X3**

The ESP32-C3 has limited RAM. Try a simpler edition of the book. Book Pet fails
cleanly rather than risking the pet save or SD contents.
