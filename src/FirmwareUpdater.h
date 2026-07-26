#pragma once

#include <Arduino.h>
#include <SHA2Builder.h>
#include <Stream.h>

#ifndef BOOKPET_REQUIRE_SIGNED_UPDATES
#define BOOKPET_REQUIRE_SIGNED_UPDATES 0
#endif

namespace bookpet {

class FirmwareUpdater {
 public:
  bool begin(size_t totalBytes, const char* expectedSha256 = nullptr);
  size_t write(const uint8_t* data, size_t length);
  bool finish();
  void abort(const char* reason = nullptr);
  bool install(Stream& source, size_t totalBytes,
               const char* expectedSha256 = nullptr);

  bool running() const { return running_; }
  bool succeeded() const { return succeeded_; }
  uint8_t progressPercent() const;
  const char* error() const { return error_; }
  String calculatedSha256() const { return calculatedSha256_; }

  static bool requiresSignature() {
    return BOOKPET_REQUIRE_SIGNED_UPDATES != 0;
  }
  static bool rollbackAvailable();
  static bool rollbackAndReboot();
  static bool confirmRunningImage();
  static bool runningImagePendingVerify();

 private:
  void setError(const char* message);

  SHA256Builder sha256_;
  String expectedSha256_;
  String calculatedSha256_;
  size_t totalBytes_ = 0;
  size_t writtenBytes_ = 0;
  bool running_ = false;
  bool succeeded_ = false;
  char error_[112] = {};
};

extern FirmwareUpdater firmwareUpdater;

}  // namespace bookpet
