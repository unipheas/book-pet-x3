#pragma once

#include <Arduino.h>

namespace bookpet {

constexpr bool isForwardReadingPosition(bool hasRewardedPage,
                                        uint16_t furthestSpine,
                                        uint32_t furthestChar,
                                        uint16_t spine,
                                        uint32_t charStart) {
  return !hasRewardedPage || spine > furthestSpine ||
         (spine == furthestSpine && charStart > furthestChar);
}
static_assert(isForwardReadingPosition(false, 0, 0, 0, 0));
static_assert(!isForwardReadingPosition(true, 0, 100, 0, 100));
static_assert(!isForwardReadingPosition(true, 2, 50, 1, 500));
static_assert(isForwardReadingPosition(true, 1, 500, 2, 0));
static_assert(isForwardReadingPosition(true, 2, 50, 2, 51));

enum class ReadingRewardKind : uint8_t { None, Page, Finish };

struct BookProgress {
  uint32_t version = 2;
  uint64_t bookId = 0;
  uint32_t charStart = 0;
  uint32_t furthestChar = 0;
  uint16_t spine = 0;
  uint16_t furthestSpine = 0;
  bool hasRewardedPage = false;
  bool finished = false;
  uint16_t checksum = 0;
};
static_assert(sizeof(BookProgress) == 32);

constexpr uint32_t mixProgressChecksum(uint32_t hash, uint64_t value,
                                       uint8_t bytes) {
  for (uint8_t i = 0; i < bytes; ++i) {
    hash ^= static_cast<uint8_t>(value >> (i * 8));
    hash *= 16777619U;
  }
  return hash;
}

constexpr uint16_t bookProgressChecksum(const BookProgress& book) {
  uint32_t hash = 2166136261U;
  hash = mixProgressChecksum(hash, book.version, sizeof(book.version));
  hash = mixProgressChecksum(hash, book.bookId, sizeof(book.bookId));
  hash = mixProgressChecksum(hash, book.charStart, sizeof(book.charStart));
  hash =
      mixProgressChecksum(hash, book.furthestChar, sizeof(book.furthestChar));
  hash = mixProgressChecksum(hash, book.spine, sizeof(book.spine));
  hash =
      mixProgressChecksum(hash, book.furthestSpine, sizeof(book.furthestSpine));
  hash = mixProgressChecksum(hash, book.hasRewardedPage ? 1 : 0, 1);
  hash = mixProgressChecksum(hash, book.finished ? 1 : 0, 1);
  const uint16_t folded =
      static_cast<uint16_t>((hash >> 16) ^ (hash & 0xFFFFU));
  return folded == 0 ? 1 : folded;
}

constexpr BookProgress kChecksumExample{};
static_assert(bookProgressChecksum(kChecksumExample) != 0);
constexpr BookProgress checksumExampleAt(uint32_t charStart) {
  BookProgress book;
  book.charStart = charStart;
  return book;
}
static_assert(bookProgressChecksum(checksumExampleAt(1)) !=
              bookProgressChecksum(checksumExampleAt(2)));

struct ReadingProgressState {
  uint32_t version = 3;
  uint64_t currentBookId = 0;
  uint32_t nextTransaction = 1;
  uint32_t pendingTransaction = 0;
  uint64_t pendingBookId = 0;
  uint32_t pendingCharStart = 0;
  uint16_t pendingSpine = 0;
  ReadingRewardKind pendingKind = ReadingRewardKind::None;
  uint8_t reserved = 0;
};
static_assert(sizeof(ReadingProgressState) == 40);

class ReadingProgress {
 public:
  bool begin();
  bool resume(uint64_t bookId, uint16_t* spine, uint32_t* charStart) const;
  bool savePosition(uint64_t bookId, uint16_t spine, uint32_t charStart);

  // Starts a durable transaction before the pet reward is changed. The caller
  // gives the returned id to PetEngine, then calls commit(). A reboot can replay
  // an unfinished transaction safely because PetEngine remembers the last id.
  uint32_t beginPageReward(uint64_t bookId, uint16_t spine,
                           uint32_t charStart);
  uint32_t beginFinishReward(uint64_t bookId, uint16_t spine,
                             uint32_t charStart);
  bool pending(uint32_t* transaction, ReadingRewardKind* kind) const;
  bool commit(uint32_t transaction);

  bool finished(uint64_t bookId) const;
  uint64_t currentBookId() const { return state_.currentBookId; }
  const char* error() const { return error_; }

 private:
  static void keyFor(uint64_t bookId, char* out, size_t cap);
  static bool pathFor(uint64_t bookId, char prefix, char* out, size_t cap);
  bool loadBook(uint64_t bookId, BookProgress* out) const;
  bool saveBook(const BookProgress& book) const;
  bool saveState() const;
  void setError(const char* message) const;
  void clearError() const { error_[0] = '\0'; }
  uint32_t beginReward(uint64_t bookId, uint16_t spine,
                       uint32_t charStart, ReadingRewardKind kind);

  ReadingProgressState state_;
  bool storageReady_ = false;
  mutable char error_[80] = {};
};

}  // namespace bookpet
