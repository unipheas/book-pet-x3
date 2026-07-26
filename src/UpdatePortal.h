#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "BookPetVersion.h"

#ifndef BOOKPET_UPDATE_MANIFEST_URL
#define BOOKPET_UPDATE_MANIFEST_URL \
  "https://unipheas.github.io/book-pet-x3/update/stable.json"
#endif

namespace bookpet {

class UpdatePortal {
 public:
  using StatusCallback =
      void (*)(const char* title, const char* detail, uint8_t progress);

  bool start(StatusCallback callback = nullptr);
  void handle();
  void stop();
  bool active() const { return active_; }
  bool safeToStop() const;
  const String& ssid() const { return apSsid_; }
  const String& password() const { return apPassword_; }
  const char* title() const { return statusTitle_; }
  const char* detail() const { return statusDetail_; }
  uint8_t progress() const { return progress_; }

 private:
  void configureRoutes();
  void setStatus(const char* title, const char* detail, uint8_t progress = 0);
  void sendStatus();
  void handleUpload();
  void performOfficialUpdate();
  bool syncSecureClock();
  bool originAllowed() const;
  static int compareVersions(const char* left, const char* right);

  DNSServer dns_;
  WebServer server_{80};
  StatusCallback statusCallback_ = nullptr;
  String apSsid_;
  String apPassword_;
  String wifiSsid_;
  String wifiPassword_;
  char statusTitle_[48] = "PHONE UPDATE";
  char statusDetail_[160] = "Starting";
  uint8_t progress_ = 0;
  bool active_ = false;
  bool uploadAccepted_ = false;
  bool uploadSucceeded_ = false;
  bool officialPending_ = false;
  bool rebootPending_ = false;
  uint32_t rebootAtMs_ = 0;
};

extern UpdatePortal updatePortal;

}  // namespace bookpet
