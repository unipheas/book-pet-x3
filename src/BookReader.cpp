#include "BookReader.h"
#include "ReaderFileFilter.h"

#include <layout/ChapterLayout.h>
#include <render/PageRenderer.h>

#include <cstdlib>

namespace bookpet {
namespace {
using namespace freeink::book;

constexpr char kBooksDirectory[] = "/BOOKS";
constexpr char kCacheRoot[] = "/BOOKPET";
constexpr char kBookCacheRoot[] = "/BOOKPET/CACHE";
constexpr size_t kCatalogBuildBytes = 96 * 1024;
constexpr size_t kParseBytes = 50 * 1024;
constexpr size_t kLayoutBytes = 108 * 1024;
constexpr size_t kIndexBytes = 36 * 1024;
constexpr size_t kPageBytes = 64 * 1024;
constexpr size_t kMaxCatalogResidentBytes = 36 * 1024;
constexpr uint32_t kFontFingerprint = 0x42504E53;  // "BPNS"

void copyText(char* dst, size_t cap, const char* source,
              const char* fallback) {
  if (!dst || cap == 0) return;
  snprintf(dst, cap, "%s",
           source && source[0] ? source : (fallback ? fallback : ""));
}
}  // namespace

bool SdBookSource::open(const char* path) {
  close();
  file_ = SdMan.open(path, O_RDONLY);
  if (!file_) return false;
  size_ = file_.fileSize();
  return size_ > 0;
}

void SdBookSource::close() {
  if (file_) file_.close();
  size_ = 0;
}

int32_t SdBookSource::readAt(uint64_t offset, void* dst, uint32_t len) {
  if (!file_ || !file_.seekSet(offset)) return -1;
  return file_.read(dst, len);
}

void SdBookCache::setDirectory(const char* path) {
  abortWrite();
  snprintf(directory_, sizeof(directory_), "%s", path ? path : "");
}

bool SdBookCache::pathFor(const char* name, char* out, size_t cap) const {
  if (!ready() || !name || !name[0] || strchr(name, '/')) return false;
  const int needed = snprintf(out, cap, "%s/%s", directory_, name);
  return needed > 0 && static_cast<size_t>(needed) < cap;
}

bool SdBookCache::exists(const char* name) {
  char path[96];
  return pathFor(name, path, sizeof(path)) && SdMan.exists(path);
}

bool SdBookCache::remove(const char* name) {
  char path[96];
  return pathFor(name, path, sizeof(path)) && SdMan.remove(path);
}

int64_t SdBookCache::fileSize(const char* name) {
  char path[96];
  if (!pathFor(name, path, sizeof(path))) return -1;
  FsFile file = SdMan.open(path, O_RDONLY);
  if (!file) return -1;
  const uint64_t size = file.fileSize();
  file.close();
  return size <= INT64_MAX ? static_cast<int64_t>(size) : -1;
}

int32_t SdBookCache::readAt(const char* name, uint32_t offset, void* dst,
                            uint32_t len) {
  char path[96];
  if (!pathFor(name, path, sizeof(path))) return -1;
  FsFile file = SdMan.open(path, O_RDONLY);
  if (!file || !file.seekSet(offset)) {
    if (file) file.close();
    return -1;
  }
  const int32_t read = file.read(dst, len);
  file.close();
  return read;
}

bool SdBookCache::beginWrite(const char* name) {
  abortWrite();
  if (!pathFor(name, targetPath_, sizeof(targetPath_))) return false;
  const int needed =
      snprintf(tempPath_, sizeof(tempPath_), "%s.tmp", targetPath_);
  if (needed <= 0 || static_cast<size_t>(needed) >= sizeof(tempPath_)) {
    return false;
  }
  if (SdMan.exists(tempPath_)) SdMan.remove(tempPath_);
  writeFile_ = SdMan.open(tempPath_, O_RDWR | O_CREAT | O_TRUNC);
  return static_cast<bool>(writeFile_);
}

bool SdBookCache::write(const void* data, uint32_t len) {
  return writeFile_ && writeFile_.write(data, len) == len;
}

bool SdBookCache::endWrite() {
  if (!writeFile_) return false;
  const bool synced = writeFile_.sync();
  writeFile_.close();
  if (!synced) {
    SdMan.remove(tempPath_);
    return false;
  }
  if (SdMan.exists(targetPath_) && !SdMan.remove(targetPath_)) {
    SdMan.remove(tempPath_);
    return false;
  }
  if (!SdMan.rename(tempPath_, targetPath_)) {
    SdMan.remove(tempPath_);
    return false;
  }
  targetPath_[0] = '\0';
  tempPath_[0] = '\0';
  return true;
}

int32_t SdBookCache::readBackAt(uint32_t offset, void* dst, uint32_t len) {
  if (!writeFile_ || !writeFile_.sync()) return -1;
  FsFile reader = SdMan.open(tempPath_, O_RDONLY);
  if (!reader || !reader.seekSet(offset)) {
    if (reader) reader.close();
    return -1;
  }
  const int32_t read = reader.read(dst, len);
  reader.close();
  return read;
}

void SdBookCache::abortWrite() {
  if (writeFile_) writeFile_.close();
  if (tempPath_[0] && SdMan.exists(tempPath_)) SdMan.remove(tempPath_);
  targetPath_[0] = '\0';
  tempPath_[0] = '\0';
}

bool BookReader::mount() {
  if (mounted_ && SdMan.ready()) {
    FsFile root = SdMan.open("/", O_RDONLY);
    const bool healthy = root && root.isDirectory();
    if (root) root.close();
    if (healthy) return true;
    mounted_ = false;
  }
  mounted_ = SdMan.begin();
  if (!mounted_) setError("SD card not found");
  return mounted_;
}

bool BookReader::isEpub(const char* name) {
  return isReadableEpubName(name);
}

const char* BookReader::displayName(const char* path) {
  if (!path) return "";
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

bool BookReader::scan() {
  close();
  bookCount_ = 0;
  memset(bookIds_, 0, sizeof(bookIds_));
  error_[0] = '\0';
  if (!mount()) return false;
  if (!SdMan.ensureDirectoryExists(kBooksDirectory)) {
    setError("Could not open or create /BOOKS");
    return false;
  }
  FsFile directory = SdMan.open(kBooksDirectory, O_RDONLY);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    setError("The /BOOKS folder could not be read");
    return false;
  }
  uint16_t epubCandidates = 0;
  uint16_t skippedEpubs = 0;
  while (bookCount_ < kMaxBooks) {
    FsFile file = directory.openNextFile();
    if (!file) break;
    if (file.isDirectory()) {
      file.close();
      continue;
    }
    char name[176] = {};
    file.getName(name, sizeof(name));
    file.close();
    if (!isEpub(name)) continue;
    epubCandidates++;
    const int needed =
        snprintf(bookPaths_[bookCount_], sizeof(bookPaths_[bookCount_]),
                 "%s/%s", kBooksDirectory, name);
    if (needed <= 0 ||
        static_cast<size_t>(needed) >= sizeof(bookPaths_[bookCount_])) {
      skippedEpubs++;
      Serial.printf("[bookpet] reader: skipped long EPUB filename: %s\n",
                    name);
      continue;
    }
    SdBookSource fingerprintSource;
    if (!fingerprintSource.open(bookPaths_[bookCount_])) {
      skippedEpubs++;
      Serial.printf("[bookpet] reader: skipped unreadable EPUB: %s\n",
                    bookPaths_[bookCount_]);
      continue;
    }
    const uint64_t bookId = hashContainer(fingerprintSource);
    fingerprintSource.close();
    if (bookId == 0) {
      skippedEpubs++;
      Serial.printf("[bookpet] reader: skipped invalid EPUB: %s\n",
                    bookPaths_[bookCount_]);
      continue;
    }
    bookIds_[bookCount_] = bookId;
    bookCount_++;
  }
  directory.close();
  if (epubCandidates > 0 && bookCount_ == 0) {
    setError("No readable EPUB files were found");
    return false;
  }
  if (skippedEpubs > 0) {
    Serial.printf("[bookpet] reader: skipped %u invalid EPUB file(s)\n",
                  static_cast<unsigned>(skippedEpubs));
  }
  for (uint8_t i = 0; i < bookCount_; ++i) {
    for (uint8_t j = i + 1; j < bookCount_; ++j) {
      if (strcasecmp(bookPaths_[i], bookPaths_[j]) <= 0) continue;
      char swap[sizeof(bookPaths_[0])];
      memcpy(swap, bookPaths_[i], sizeof(swap));
      memcpy(bookPaths_[i], bookPaths_[j], sizeof(bookPaths_[i]));
      memcpy(bookPaths_[j], swap, sizeof(bookPaths_[j]));
      const uint64_t swapId = bookIds_[i];
      bookIds_[i] = bookIds_[j];
      bookIds_[j] = swapId;
    }
  }
  return true;
}

const char* BookReader::bookName(uint8_t index) const {
  if (index >= bookCount_) return "";
  return displayName(bookPaths_[index]);
}

uint64_t BookReader::hashContainer(BookSource& source) {
  uint32_t directoryOffset = 0;
  uint32_t directorySize = 0;
  uint16_t entryCount = 0;
  const BookStatus status = ZipCatalog::locateCentralDirectory(
      source, &directoryOffset, &directorySize, &entryCount);
  if (status != BookStatus::Ok) {
    Serial.printf("[bookpet] reader: EPUB probe=%s size=%llu\n",
                  bookStatusName(status),
                  static_cast<unsigned long long>(source.size()));
    return 0;
  }
  uint64_t hash = 1469598103934665603ULL;
  uint8_t buffer[1024];
  uint32_t done = 0;
  while (done < directorySize) {
    const uint32_t wanted =
        min<uint32_t>(sizeof(buffer), directorySize - done);
    const int32_t count =
        source.readAt(static_cast<uint64_t>(directoryOffset) + done,
                      buffer, wanted);
    if (count != static_cast<int32_t>(wanted)) return 0;
    for (int32_t i = 0; i < count; ++i) {
      hash ^= buffer[i];
      hash *= 1099511628211ULL;
    }
    done += wanted;
  }
  const uint64_t size = source.size();
  const uint32_t geometry[] = {
      directoryOffset, directorySize, entryCount,
      static_cast<uint32_t>(size), static_cast<uint32_t>(size >> 32)};
  const uint8_t* geometryBytes =
      reinterpret_cast<const uint8_t*>(geometry);
  for (size_t i = 0; i < sizeof(geometry); ++i) {
    hash ^= geometryBytes[i];
    hash *= 1099511628211ULL;
  }
  return hash ? hash : 1ULL;
}

uint64_t BookReader::bookIdAt(uint8_t index) const {
  return index < bookCount_ ? bookIds_[index] : 0;
}

int BookReader::findBook(uint64_t bookId) const {
  if (!bookId) return -1;
  for (uint8_t i = 0; i < bookCount_; ++i) {
    if (bookIdAt(i) == bookId) return i;
  }
  return -1;
}

bool BookReader::allocate(void** target, size_t bytes) {
  *target = malloc(bytes);
  if (*target) return true;
  setError("Not enough memory to open this book");
  return false;
}

void BookReader::setError(const char* message) {
  snprintf(error_, sizeof(error_), "%s", message ? message : "Book error");
  Serial.printf("[bookpet] reader: %s\n", error_);
}

bool BookReader::prepareCatalog() {
  if (!SdMan.ensureDirectoryExists(kCacheRoot) ||
      !SdMan.ensureDirectoryExists(kBookCacheRoot)) {
    setError("Could not create the Book Pet cache");
    return false;
  }
  char cacheDirectory[48];
  snprintf(cacheDirectory, sizeof(cacheDirectory), "%s/%08lX%08lX",
           kBookCacheRoot,
           static_cast<unsigned long>(bookId_ >> 32),
           static_cast<unsigned long>(bookId_ & 0xFFFFFFFFULL));
  if (!SdMan.ensureDirectoryExists(cacheDirectory)) {
    setError("Could not create this book's cache");
    return false;
  }
  cache_.setDirectory(cacheDirectory);

  bool catalogBuilt = false;
  auto buildCatalog = [&]() -> bool {
    void* buildBuffer = nullptr;
    void* parseBuffer = nullptr;
    const bool borrowedParse =
        buildStorage_ != nullptr && buildStorageBytes_ >= kParseBytes;
    if (!allocate(&buildBuffer, kCatalogBuildBytes) ||
        (!borrowedParse && !allocate(&parseBuffer, kParseBytes))) {
      free(buildBuffer);
      free(parseBuffer);
      return false;
    }
    if (borrowedParse) parseBuffer = buildStorage_;
    Arena buildArena(buildBuffer, kCatalogBuildBytes);
    Arena parseArena(parseBuffer, kParseBytes);
    const BookStatus status =
        BookCatalog::build(source_, cache_, buildArena, &parseArena);
    Serial.printf(
        "[bookpet] catalog build=%s layout=%u parse=%u heap=%u\n",
        bookStatusName(status), static_cast<unsigned>(buildArena.highWater()),
        static_cast<unsigned>(parseArena.highWater()),
        static_cast<unsigned>(ESP.getFreeHeap()));
    free(buildBuffer);
    if (!borrowedParse) free(parseBuffer);
    if (status != BookStatus::Ok) {
      setError(status == BookStatus::Encrypted
                   ? "This EPUB is DRM protected"
                   : "This EPUB could not be indexed");
      return false;
    }
    catalogBuilt = true;
    return true;
  };

  if (!cache_.exists(BookCatalog::kCatalogName) && !buildCatalog()) {
    return false;
  }

  size_t resident = 0;
  BookStatus status = BookCatalog::residentBytes(cache_, &resident);
  if (status != BookStatus::Ok) {
    cache_.remove(BookCatalog::kCatalogName);
    if (!buildCatalog() ||
        BookCatalog::residentBytes(cache_, &resident) != BookStatus::Ok) {
      setError("The EPUB index could not be rebuilt");
      return false;
    }
  }
  if (resident == 0 || resident > kMaxCatalogResidentBytes) {
    setError("This EPUB has too many chapters for X3");
    return false;
  }

  catalogBytes_ = resident + 256;
  if (!allocate(&catalogBuffer_, catalogBytes_)) return false;
  catalogArena_.init(catalogBuffer_, catalogBytes_);
  uint8_t fingerprintScratch[4096];
  Arena scratch(fingerprintScratch, sizeof(fingerprintScratch));
  status = catalog_.open(source_, cache_, catalogArena_, scratch);
  if (status != BookStatus::Ok && !catalogBuilt) {
    releaseCatalog();
    cache_.remove(BookCatalog::kCatalogName);
    if (!buildCatalog() ||
        BookCatalog::residentBytes(cache_, &resident) != BookStatus::Ok ||
        resident == 0 || resident > kMaxCatalogResidentBytes) {
      setError("The EPUB index could not be refreshed");
      return false;
    }
    catalogBytes_ = resident + 256;
    if (!allocate(&catalogBuffer_, catalogBytes_)) return false;
    catalogArena_.init(catalogBuffer_, catalogBytes_);
    scratch.reset();
    status = catalog_.open(source_, cache_, catalogArena_, scratch);
  }
  if (status != BookStatus::Ok || catalog_.spineCount() == 0) {
    setError(status == BookStatus::Encrypted
                 ? "This EPUB is DRM protected"
                 : "This EPUB has no readable chapters");
    return false;
  }

  copyText(title_, sizeof(title_), catalog_.metadata().title,
           displayName(bookPaths_[selected_]));
  copyText(author_, sizeof(author_), catalog_.metadata().author,
           "Unknown author");
  return true;
}

bool BookReader::prepareChapter(uint16_t requestedSpine,
                                uint32_t resumeChar) {
  return prepareChapterInternal(requestedSpine, resumeChar, true);
}

bool BookReader::prepareChapterInternal(uint16_t requestedSpine,
                                        uint32_t resumeChar,
                                        bool restoreOnFailure) {
  const bool hadChapter = pageReader_.pageCount() > 0;
  const uint16_t previousSpine = spine_;
  const uint32_t previousChar = hadChapter ? charStart() : 0;
  releaseChapter();
  auto fail = [&]() -> bool {
    char requestedError[sizeof(error_)];
    memcpy(requestedError, error_, sizeof(requestedError));
    if (restoreOnFailure && hadChapter) {
      if (!prepareChapterInternal(previousSpine, previousChar, false)) {
        open_ = false;
      }
      memcpy(error_, requestedError, sizeof(error_));
    }
    return false;
  };
  if (!catalog_.isOpen() || requestedSpine >= catalog_.spineCount()) {
    setError("Chapter not found");
    return fail();
  }
  spine_ = requestedSpine;

  LayoutParams params;
  params.pageWidth = 528;
  params.pageHeight = 792;
  params.marginLeft = 34;
  params.marginRight = 34;
  params.marginTop = 68;
  params.marginBottom = 72;
  params.baseSizePx = 18;
  params.language = catalog_.metadata().language[0]
                        ? catalog_.metadata().language
                        : "en";
  params.font = &fonts_;
  params.defaultAlign = TextAlign::Left;
  params.lineSpacingPct = 105;
  params.paragraphSpacingPct = 80;
  params.stylesheet = catalog_.stylesheet();
  const uint32_t generation =
      layoutGenerationHash(params, kFontFingerprint);
  char cacheName[48];
  if (!pageCacheName(spine_, generation, cacheName, sizeof(cacheName))) {
    setError("Chapter cache name failed");
    return fail();
  }

  auto buildPageCache = [&]() -> bool {
    void* layoutBuffer = nullptr;
    void* parseBuffer = nullptr;
    const bool borrowedParse =
        buildStorage_ != nullptr && buildStorageBytes_ >= kParseBytes;
    if (!allocate(&layoutBuffer, kLayoutBytes) ||
        (!borrowedParse && !allocate(&parseBuffer, kParseBytes))) {
      free(layoutBuffer);
      free(parseBuffer);
      return false;
    }
    if (borrowedParse) parseBuffer = buildStorage_;
    Arena layoutArena(layoutBuffer, kLayoutBytes);
    Arena parseArena(parseBuffer, kParseBytes);
    PageCacheWriter writer;
    ZipEntry entry;
    char href[384] = {};
    BookStatus status = catalog_.spineEntry(spine_, &entry);
    if (status == BookStatus::Ok) {
      status = catalog_.spineHref(spine_, href, sizeof(href));
    }
    if (status == BookStatus::Ok &&
        !writer.begin(cache_, cacheName, generation, layoutArena)) {
      status = BookStatus::IoError;
    }
    uint32_t totalChars = 0;
    if (status == BookStatus::Ok) {
      status = ChapterLayout::layout(
          source_, catalog_.zip(), entry, href, params, layoutArena, writer,
          nullptr, &totalChars, &parseArena);
    }
    if (status == BookStatus::Ok) {
      writer.setTotalChars(totalChars);
      if (!writer.finish()) status = BookStatus::IoError;
    }
    if (status != BookStatus::Ok) {
      cache_.cancelWrite();
      cache_.remove(cacheName);
    }
    Serial.printf(
        "[bookpet] chapter=%u layout=%s layout_mem=%u parse_mem=%u heap=%u\n",
        spine_, bookStatusName(status),
        static_cast<unsigned>(layoutArena.highWater()),
        static_cast<unsigned>(parseArena.highWater()),
        static_cast<unsigned>(ESP.getFreeHeap()));
    free(layoutBuffer);
    if (!borrowedParse) free(parseBuffer);
    if (status != BookStatus::Ok) {
      setError(status == BookStatus::OutOfMemory
                   ? "This chapter is too complex for X3"
                   : "This chapter could not be prepared");
      return false;
    }
    return true;
  };

  if (!cache_.exists(cacheName) && !buildPageCache()) {
    return fail();
  }

  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    if (!allocate(&indexBuffer_, kIndexBytes) ||
        !allocate(&pageBuffer_, kPageBytes)) {
      releaseChapter();
      return fail();
    }
    indexArena_.init(indexBuffer_, kIndexBytes);
    pageArena_.init(pageBuffer_, kPageBytes);
    const BookStatus openStatus =
        pageReader_.open(cache_, cacheName, generation, indexArena_);
    if (openStatus == BookStatus::Ok && pageReader_.pageCount() > 0) {
      pageIndex_ = pageReader_.pageForChar(resumeChar);
      if (pageIndex_ >= pageReader_.pageCount()) pageIndex_ = 0;
      return true;
    }
    releaseChapter();
    if (openStatus == BookStatus::OutOfMemory) {
      setError("This chapter has too many pages");
      return fail();
    }
    cache_.remove(cacheName);
    if (attempt == 0 && buildPageCache()) continue;
    setError("Chapter cache could not be rebuilt");
    return fail();
  }
  setError("Chapter could not be opened");
  return fail();
}

bool BookReader::open(uint8_t index, uint16_t resumeSpine,
                      uint32_t resumeChar) {
  close();
  error_[0] = '\0';
  if (index >= bookCount_ || !mount()) {
    setError("Book not found");
    return false;
  }
  selected_ = index;
  if (!source_.open(bookPaths_[index])) {
    setError("Could not open the EPUB");
    return false;
  }
  bookId_ = bookIds_[index];
  fonts_ = {};
  fonts_.add(&bitmapFont_);
  if (!prepareCatalog()) {
    close();
    return false;
  }
  if (resumeSpine >= catalog_.spineCount()) {
    resumeSpine = 0;
    resumeChar = 0;
  }
  if (!prepareChapter(resumeSpine, resumeChar)) {
    close();
    return false;
  }
  open_ = true;
  return true;
}

void BookReader::releaseChapter() {
  pageReader_ = {};
  free(indexBuffer_);
  free(pageBuffer_);
  indexBuffer_ = nullptr;
  pageBuffer_ = nullptr;
  indexArena_.init(nullptr, 0);
  pageArena_.init(nullptr, 0);
  pageIndex_ = 0;
}

void BookReader::releaseCatalog() {
  catalog_ = {};
  free(catalogBuffer_);
  catalogBuffer_ = nullptr;
  catalogBytes_ = 0;
  catalogArena_.init(nullptr, 0);
}

void BookReader::close() {
  open_ = false;
  releaseChapter();
  releaseCatalog();
  source_.close();
  cache_.setDirectory(nullptr);
  bookId_ = 0;
  spine_ = 0;
}

uint32_t BookReader::charStart() const {
  return pageReader_.pageCount() > 0
             ? pageReader_.charStart(pageIndex_)
             : 0;
}

bool BookReader::nextPage() {
  if (!open_) return false;
  if (pageIndex_ + 1 < pageReader_.pageCount()) {
    pageIndex_++;
    return true;
  }
  if (spine_ + 1 >= catalog_.spineCount()) return false;
  return prepareChapter(spine_ + 1, 0);
}

bool BookReader::previousPage() {
  if (!open_) return false;
  if (pageIndex_ > 0) {
    pageIndex_--;
    return true;
  }
  if (spine_ == 0 || !prepareChapter(spine_ - 1, UINT32_MAX)) return false;
  pageIndex_ = pageReader_.pageCount() - 1;
  return true;
}

bool BookReader::nextChapter() {
  if (!open_ || spine_ + 1 >= catalog_.spineCount()) return false;
  return prepareChapter(spine_ + 1, 0);
}

bool BookReader::previousChapter() {
  if (!open_ || spine_ == 0) return false;
  return prepareChapter(spine_ - 1, 0);
}

bool BookReader::atBookEnd() const {
  return open_ && spine_ + 1 >= catalog_.spineCount() &&
         pageReader_.pageCount() > 0 &&
         pageIndex_ + 1 >= pageReader_.pageCount();
}

bool BookReader::render(uint8_t* framebuffer, int16_t panelWidth,
                        int16_t panelHeight) {
  if (!open_ || !framebuffer || pageReader_.pageCount() == 0) return false;
  pageArena_.reset();
  Page page;
  const BookStatus readStatus =
      pageReader_.readPage(pageIndex_, pageArena_, &page);
  if (readStatus != BookStatus::Ok) {
    setError("This page could not be read");
    return false;
  }
  FrameTarget target{framebuffer,
                     panelWidth,
                     panelHeight,
                     static_cast<int16_t>((panelWidth + 7) / 8),
                     FrameFormat::Mono1Dithered,
                     FrameRotation::Portrait};
  const BookStatus renderStatus = PageRenderer::render(
      page, fonts_, source_, catalog_.zip(), pageArena_, target);
  if (renderStatus != BookStatus::Ok) {
    setError("This page could not be drawn");
    return false;
  }
  return true;
}

}  // namespace bookpet
