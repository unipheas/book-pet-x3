#include "ReadingProgress.h"

#include <Preferences.h>
#include <SPIFFS.h>

namespace bookpet {
namespace {
constexpr char kNamespace[] = "bookpet";
constexpr char kStateKey[] = "reading";
}

void ReadingProgress::keyFor(uint64_t bookId, char* out, size_t cap) {
  static constexpr char kBase32[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
  if (!out || cap < 15) return;
  out[0] = 'p';
  for (int8_t i = 13; i >= 1; --i) {
    out[i] = kBase32[bookId & 31U];
    bookId >>= 5;
  }
  out[14] = '\0';
}

bool ReadingProgress::pathFor(uint64_t bookId, char prefix, char* out,
                              size_t cap) {
  char key[15] = {};
  keyFor(bookId, key, sizeof(key));
  const int needed = snprintf(out, cap, "/r%c%s", prefix, key + 1);
  return needed > 0 && static_cast<size_t>(needed) < cap;
}

void ReadingProgress::setError(const char* message) const {
  snprintf(error_, sizeof(error_), "%s",
           message ? message : "Reading progress could not be saved");
  Serial.printf("[bookpet] progress: %s\n", error_);
}

bool ReadingProgress::begin() {
  clearError();
  storageReady_ = SPIFFS.begin(false);
  if (!storageReady_) storageReady_ = SPIFFS.begin(true);
  if (!storageReady_) {
    storageReady_ = false;
    setError("Reading storage could not be opened");
  }
  Preferences prefs;
  prefs.begin(kNamespace, true);
  if (prefs.getBytesLength(kStateKey) == sizeof(state_)) {
    ReadingProgressState saved;
    prefs.getBytes(kStateKey, &saved, sizeof(saved));
    if (saved.version == 3) state_ = saved;
  }
  prefs.end();
  return storageReady_;
}

bool ReadingProgress::loadBook(uint64_t bookId, BookProgress* out) const {
  if (!out || bookId == 0 || !storageReady_) return false;
  *out = {};
  out->bookId = bookId;
  char target[32] = {};
  char backup[32] = {};
  pathFor(bookId, 'p', target, sizeof(target));
  pathFor(bookId, 'b', backup, sizeof(backup));
  auto loadPath = [&](const char* path) -> bool {
    if (!SPIFFS.exists(path)) return false;
    File file = SPIFFS.open(path, FILE_READ);
    bool valid = false;
    if (file && file.size() == sizeof(*out)) {
      BookProgress saved;
      if (file.read(reinterpret_cast<uint8_t*>(&saved), sizeof(saved)) ==
              sizeof(saved) &&
          saved.version == 1 && saved.bookId == bookId) {
        *out = saved;
        valid = true;
      }
    }
    if (file) file.close();
    return valid;
  };
  if (loadPath(target)) return true;
  *out = {};
  out->bookId = bookId;
  return loadPath(backup);
}

bool ReadingProgress::saveBook(const BookProgress& book) const {
  if (book.bookId == 0 || !storageReady_) {
    setError("Reading storage is not available");
    return false;
  }
  char target[32] = {};
  char temp[32] = {};
  char backup[32] = {};
  pathFor(book.bookId, 'p', target, sizeof(target));
  pathFor(book.bookId, 't', temp, sizeof(temp));
  pathFor(book.bookId, 'b', backup, sizeof(backup));
  if (!SPIFFS.exists(target) && SPIFFS.exists(backup) &&
      !SPIFFS.rename(backup, target)) {
    setError("Reading progress recovery could not finish");
    return false;
  }
  SPIFFS.remove(temp);
  File file = SPIFFS.open(temp, FILE_WRITE);
  const bool written =
      file && file.write(reinterpret_cast<const uint8_t*>(&book),
                         sizeof(book)) == sizeof(book);
  if (file) {
    file.flush();
    file.close();
  }
  if (!written) {
    SPIFFS.remove(temp);
    setError("Reading progress could not be written");
    return false;
  }
  SPIFFS.remove(backup);
  const bool hadTarget = SPIFFS.exists(target);
  if (hadTarget && !SPIFFS.rename(target, backup)) {
    SPIFFS.remove(temp);
    setError("Reading progress could not be replaced");
    return false;
  }
  if (!SPIFFS.rename(temp, target)) {
    if (hadTarget) SPIFFS.rename(backup, target);
    SPIFFS.remove(temp);
    setError("Reading progress could not be committed");
    return false;
  }
  SPIFFS.remove(backup);
  return true;
}

bool ReadingProgress::saveState() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    setError("Reading journal could not be opened");
    return false;
  }
  const bool saved =
      prefs.putBytes(kStateKey, &state_, sizeof(state_)) == sizeof(state_);
  prefs.end();
  if (!saved) setError("Reading journal could not be saved");
  return saved;
}

bool ReadingProgress::resume(uint64_t bookId, uint16_t* spine,
                             uint32_t* charStart) const {
  BookProgress book;
  if (!loadBook(bookId, &book)) return false;
  if (spine) *spine = book.spine;
  if (charStart) *charStart = book.charStart;
  return true;
}

bool ReadingProgress::savePosition(uint64_t bookId, uint16_t spine,
                                   uint32_t charStart) {
  clearError();
  BookProgress book;
  loadBook(bookId, &book);
  book.spine = spine;
  book.charStart = charStart;
  const bool currentChanged = state_.currentBookId != bookId;
  state_.currentBookId = bookId;
  return saveBook(book) && (!currentChanged || saveState());
}

uint32_t ReadingProgress::beginReward(uint64_t bookId, uint16_t spine,
                                      uint32_t charStart,
                                      ReadingRewardKind kind) {
  clearError();
  if (bookId == 0 || kind == ReadingRewardKind::None ||
      state_.pendingTransaction != 0) {
    return 0;
  }
  BookProgress book;
  loadBook(bookId, &book);
  if (kind == ReadingRewardKind::Page &&
      !isForwardReadingPosition(book.hasRewardedPage, book.furthestSpine,
                                book.furthestChar, spine, charStart)) {
    savePosition(bookId, spine, charStart);
    return 0;
  }
  if (kind == ReadingRewardKind::Finish && book.finished) {
    savePosition(bookId, spine, charStart);
    return 0;
  }

  uint32_t transaction = state_.nextTransaction++;
  if (transaction == 0) transaction = state_.nextTransaction++;
  state_.pendingTransaction = transaction;
  state_.pendingBookId = bookId;
  state_.pendingSpine = spine;
  state_.pendingCharStart = charStart;
  state_.pendingKind = kind;
  state_.currentBookId = bookId;
  if (!saveState()) {
    state_.pendingTransaction = 0;
    state_.pendingBookId = 0;
    state_.pendingKind = ReadingRewardKind::None;
    return 0;
  }
  return transaction;
}

uint32_t ReadingProgress::beginPageReward(uint64_t bookId, uint16_t spine,
                                          uint32_t charStart) {
  return beginReward(bookId, spine, charStart, ReadingRewardKind::Page);
}

uint32_t ReadingProgress::beginFinishReward(uint64_t bookId, uint16_t spine,
                                            uint32_t charStart) {
  return beginReward(bookId, spine, charStart, ReadingRewardKind::Finish);
}

bool ReadingProgress::pending(uint32_t* transaction,
                              ReadingRewardKind* kind) const {
  if (state_.pendingTransaction == 0 ||
      state_.pendingKind == ReadingRewardKind::None) {
    return false;
  }
  if (transaction) *transaction = state_.pendingTransaction;
  if (kind) *kind = state_.pendingKind;
  return true;
}

bool ReadingProgress::commit(uint32_t transaction) {
  clearError();
  if (transaction == 0 || transaction != state_.pendingTransaction) {
    return false;
  }
  BookProgress book;
  loadBook(state_.pendingBookId, &book);
  book.spine = state_.pendingSpine;
  book.charStart = state_.pendingCharStart;
  if (state_.pendingKind == ReadingRewardKind::Page) {
    book.hasRewardedPage = true;
    book.furthestSpine = state_.pendingSpine;
    book.furthestChar = state_.pendingCharStart;
  } else if (state_.pendingKind == ReadingRewardKind::Finish) {
    book.finished = true;
  }
  if (!saveBook(book)) return false;

  const ReadingProgressState pendingState = state_;
  state_.pendingTransaction = 0;
  state_.pendingBookId = 0;
  state_.pendingCharStart = 0;
  state_.pendingSpine = 0;
  state_.pendingKind = ReadingRewardKind::None;
  if (!saveState()) {
    state_ = pendingState;
    return false;
  }
  return true;
}

bool ReadingProgress::finished(uint64_t bookId) const {
  BookProgress book;
  return loadBook(bookId, &book) && book.finished;
}

}  // namespace bookpet
