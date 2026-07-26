#include "FirmwareUpdater.h"

#include <Update.h>
#include <esp_ota_ops.h>

#if BOOKPET_REQUIRE_SIGNED_UPDATES
#include <Updater_Signing.h>

#include "UpdatePublicKey.h"
#endif

namespace bookpet {
namespace {
constexpr size_t kMinimumFirmwareBytes = 32 * 1024;
constexpr uint32_t kStreamStallTimeoutMs = 30'000;

#if BOOKPET_REQUIRE_SIGNED_UPDATES
UpdaterRSAVerifier& releaseVerifier() {
  static UpdaterRSAVerifier verifier(
      reinterpret_cast<const uint8_t*>(kBookPetUpdatePublicKey),
      sizeof(kBookPetUpdatePublicKey) - 1, HASH_SHA256);
  return verifier;
}
#endif
}  // namespace

FirmwareUpdater firmwareUpdater;

void FirmwareUpdater::setError(const char* message) {
  snprintf(error_, sizeof(error_), "%s", message ? message : "Update failed");
  Serial.printf("[bookpet] update error: %s\n", error_);
}

bool FirmwareUpdater::begin(size_t totalBytes, const char* expectedSha256) {
  abort();
  error_[0] = '\0';
  succeeded_ = false;
  calculatedSha256_ = "";
  expectedSha256_ = expectedSha256 ? expectedSha256 : "";
  expectedSha256_.trim();
  expectedSha256_.toLowerCase();
  if (expectedSha256_.length() != 0 && expectedSha256_.length() != 64) {
    setError("The SHA-256 value is not valid");
    return false;
  }
  if (totalBytes < kMinimumFirmwareBytes) {
    setError("The firmware file is too small");
    return false;
  }

#if BOOKPET_REQUIRE_SIGNED_UPDATES
  if (!Update.installSignature(&releaseVerifier())) {
    setError("Release signature verification could not start");
    return false;
  }
#endif

  if (!Update.begin(totalBytes, U_FLASH)) {
    setError(Update.errorString());
    return false;
  }

  totalBytes_ = totalBytes;
  writtenBytes_ = 0;
  sha256_.begin();
  running_ = true;
  Serial.printf("[bookpet] update begin bytes=%u signed=%u\n",
                static_cast<unsigned>(totalBytes),
                static_cast<unsigned>(requiresSignature()));
  return true;
}

size_t FirmwareUpdater::write(const uint8_t* data, size_t length) {
  if (!running_ || data == nullptr || length == 0) return 0;
  const size_t remaining =
      totalBytes_ > writtenBytes_ ? totalBytes_ - writtenBytes_ : 0;
  if (length > remaining) length = remaining;
  const size_t written =
      Update.write(const_cast<uint8_t*>(data), length);
  if (written > 0) {
    sha256_.add(data, written);
    writtenBytes_ += written;
  }
  if (written != length) {
    setError(Update.errorString());
  }
  return written;
}

bool FirmwareUpdater::finish() {
  if (!running_) {
    if (error_[0] == '\0') setError("No update is running");
    return false;
  }
  if (writtenBytes_ != totalBytes_) {
    setError("The firmware file ended early");
    Update.abort();
    running_ = false;
    return false;
  }

  sha256_.calculate();
  calculatedSha256_ = sha256_.toString();
  calculatedSha256_.toLowerCase();
  if (expectedSha256_.length() == 64 &&
      calculatedSha256_ != expectedSha256_) {
    setError("SHA-256 verification failed");
    Update.abort();
    running_ = false;
    return false;
  }

  if (!Update.end()) {
    setError(Update.errorString());
    running_ = false;
    return false;
  }

  running_ = false;
  succeeded_ = true;
  Serial.printf("[bookpet] update verified sha256=%s\n",
                calculatedSha256_.c_str());
  return true;
}

void FirmwareUpdater::abort(const char* reason) {
  if (Update.isRunning()) Update.abort();
  running_ = false;
  totalBytes_ = 0;
  writtenBytes_ = 0;
  if (reason) setError(reason);
}

bool FirmwareUpdater::install(Stream& source, size_t totalBytes,
                              const char* expectedSha256) {
  if (!begin(totalBytes, expectedSha256)) return false;
  source.setTimeout(2'000);
  uint8_t buffer[4096];
  uint32_t lastProgressMs = millis();
  while (writtenBytes_ < totalBytes_) {
    const size_t remaining = totalBytes_ - writtenBytes_;
    const size_t requested =
        remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const size_t read = source.readBytes(buffer, requested);
    if (read == 0) {
      if (millis() - lastProgressMs >= kStreamStallTimeoutMs) {
        abort("The update source stopped responding");
        return false;
      }
      delay(10);
      continue;
    }
    lastProgressMs = millis();
    if (write(buffer, read) != read) {
      abort();
      return false;
    }
    delay(1);
  }
  return finish();
}

uint8_t FirmwareUpdater::progressPercent() const {
  if (totalBytes_ == 0) return succeeded_ ? 100 : 0;
  return static_cast<uint8_t>(
      min<size_t>(100, (writtenBytes_ * 100) / totalBytes_));
}

bool FirmwareUpdater::rollbackAvailable() {
  return Update.canRollBack();
}

bool FirmwareUpdater::rollbackAndReboot() {
  if (!Update.rollBack()) return false;
  delay(250);
  ESP.restart();
  return true;
}

bool FirmwareUpdater::runningImagePendingVerify() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  return running != nullptr &&
         esp_ota_get_state_partition(running, &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

bool FirmwareUpdater::confirmRunningImage() {
  if (!runningImagePendingVerify()) return true;
  const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
  Serial.printf("[bookpet] confirm OTA image result=%d\n",
                static_cast<int>(result));
  return result == ESP_OK;
}

}  // namespace bookpet
