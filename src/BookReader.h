#pragma once

#include <Arduino.h>
#include <BookCatalog.h>
#include <FreeInkUIBookFont.h>
#include <SDCardManager.h>
#include <cache/PageCache.h>

namespace bookpet {

class SdBookSource final : public freeink::book::BookSource {
 public:
  ~SdBookSource() override { close(); }
  bool open(const char* path);
  void close();
  int32_t readAt(uint64_t offset, void* dst, uint32_t len) override;
  uint64_t size() const override { return size_; }

 private:
  FsFile file_;
  uint64_t size_ = 0;
};

class SdBookCache final : public freeink::book::CacheStorage {
 public:
  void setDirectory(const char* path);
  bool ready() const { return directory_[0] != '\0'; }

  bool exists(const char* name) override;
  bool remove(const char* name) override;
  int64_t fileSize(const char* name) override;
  int32_t readAt(const char* name, uint32_t offset, void* dst,
                 uint32_t len) override;
  bool beginWrite(const char* name) override;
  bool write(const void* data, uint32_t len) override;
  bool endWrite() override;
  int32_t readBackAt(uint32_t offset, void* dst, uint32_t len) override;
  void cancelWrite() { abortWrite(); }

 private:
  bool pathFor(const char* name, char* out, size_t cap) const;
  void abortWrite();

  char directory_[48] = {};
  char targetPath_[96] = {};
  char tempPath_[96] = {};
  FsFile writeFile_;
};

class BookReader {
 public:
  static constexpr uint8_t kMaxBooks = 24;

  ~BookReader() { close(); }
  bool scan();
  uint8_t bookCount() const { return bookCount_; }
  const char* bookName(uint8_t index) const;
  uint64_t bookIdAt(uint8_t index) const;
  int findBook(uint64_t bookId) const;
  void setBuildStorage(void* storage, size_t bytes) {
    buildStorage_ = storage;
    buildStorageBytes_ = bytes;
  }

  bool open(uint8_t index, uint16_t resumeSpine = 0,
            uint32_t resumeChar = 0);
  void close();
  bool isOpen() const { return open_; }
  bool nextPage();
  bool previousPage();
  bool nextChapter();
  bool previousChapter();
  bool atBookEnd() const;

  bool render(uint8_t* framebuffer, int16_t panelWidth, int16_t panelHeight);
  const char* title() const { return title_; }
  const char* author() const { return author_; }
  const char* error() const { return error_; }
  uint64_t bookId() const { return bookId_; }
  uint16_t spine() const { return spine_; }
  uint32_t charStart() const;
  uint32_t chapterPage() const { return pageIndex_; }
  uint32_t chapterPages() const { return pageReader_.pageCount(); }
  uint16_t chapterCount() const {
    return static_cast<uint16_t>(catalog_.spineCount());
  }

 private:
  bool mount();
  bool prepareCatalog();
  bool prepareChapter(uint16_t spine, uint32_t resumeChar);
  bool prepareChapterInternal(uint16_t spine, uint32_t resumeChar,
                              bool restoreOnFailure);
  bool allocate(void** target, size_t bytes);
  void releaseChapter();
  void releaseCatalog();
  void setError(const char* message);
  static uint64_t hashContainer(freeink::book::BookSource& source);
  static bool isEpub(const char* name);
  static const char* displayName(const char* path);

  char bookPaths_[kMaxBooks][192] = {};
  uint64_t bookIds_[kMaxBooks] = {};
  uint8_t bookCount_ = 0;
  bool mounted_ = false;
  bool open_ = false;
  uint8_t selected_ = 0;
  uint64_t bookId_ = 0;
  uint16_t spine_ = 0;
  uint32_t pageIndex_ = 0;
  char title_[72] = {};
  char author_[56] = {};
  char error_[96] = {};

  SdBookSource source_;
  SdBookCache cache_;
  freeink::book::BookCatalog catalog_;
  freeink::book::PageCacheReader pageReader_;
  freeink::ui::BitmapBookFont bitmapFont_;
  freeink::book::FontChain fonts_;

  void* catalogBuffer_ = nullptr;
  size_t catalogBytes_ = 0;
  void* indexBuffer_ = nullptr;
  void* pageBuffer_ = nullptr;
  freeink::book::Arena catalogArena_;
  freeink::book::Arena indexArena_;
  freeink::book::Arena pageArena_;
  void* buildStorage_ = nullptr;
  size_t buildStorageBytes_ = 0;
};

}  // namespace bookpet
