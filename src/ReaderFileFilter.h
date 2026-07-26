#pragma once

#include <stddef.h>

namespace bookpet {

constexpr char asciiLower(char value) {
  return value >= 'A' && value <= 'Z'
             ? static_cast<char>(value + ('a' - 'A'))
             : value;
}

constexpr bool isReadableEpubName(const char* name) {
  if (!name || name[0] == '.') return false;
  size_t length = 0;
  while (name[length] != '\0') ++length;
  if (length < 5) return false;
  const char* extension = name + length - 5;
  return extension[0] == '.' && asciiLower(extension[1]) == 'e' &&
         asciiLower(extension[2]) == 'p' &&
         asciiLower(extension[3]) == 'u' &&
         asciiLower(extension[4]) == 'b';
}

static_assert(isReadableEpubName("The Little Prince.epub"));
static_assert(isReadableEpubName("CHARLOTTE.EPUB"));
static_assert(!isReadableEpubName("._The Little Prince.epub"));
static_assert(!isReadableEpubName(".hidden.epub"));
static_assert(!isReadableEpubName("notes.txt"));

}  // namespace bookpet
