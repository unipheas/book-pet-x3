#include "UpdatePortal.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_random.h>
#include <time.h>

#include "FirmwareUpdater.h"
#include "UpdatePortalPage.h"
#include "UpdateTrust.h"

namespace bookpet {
namespace {
constexpr uint16_t kDnsPort = 53;
constexpr uint32_t kWifiTimeoutMs = 25'000;
constexpr uint32_t kClockTimeoutMs = 20'000;
constexpr size_t kMaximumUpdateBytes = 0x680000;
constexpr char kProductId[] = "book-pet-x3";
constexpr char kAllowedUpdatePrefix[] =
    "https://unipheas.github.io/book-pet-x3/";
const char* kCollectedHeaders[] = {"Origin", "Host"};
constexpr char kPasswordAlphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";

String jsonEscape(const char* value) {
  String escaped;
  if (!value) return escaped;
  while (*value) {
    const uint8_t character = static_cast<uint8_t>(*value);
    if (*value == '"' || *value == '\\') escaped += '\\';
    if (character < 0x20) {
      escaped += ' ';
    } else {
      escaped += *value;
    }
    ++value;
  }
  return escaped;
}
}  // namespace

UpdatePortal updatePortal;

bool UpdatePortal::start(StatusCallback callback) {
  if (active_) return true;
  statusCallback_ = callback;
  const uint32_t identity =
      static_cast<uint32_t>(ESP.getEfuseMac()) & 0xFFFFFF;
  char value[32];
  snprintf(value, sizeof(value), "BookPet-%06lX",
           static_cast<unsigned long>(identity));
  apSsid_ = value;
  char password[19] = "pages-";
  for (size_t index = 6; index < sizeof(password) - 1; ++index) {
    password[index] =
        kPasswordAlphabet[esp_random() % (sizeof(kPasswordAlphabet) - 1)];
  }
  password[sizeof(password) - 1] = '\0';
  apPassword_ = password;
  char token[33];
  for (size_t index = 0; index < 4; ++index) {
    snprintf(token + index * 8, 9, "%08lx",
             static_cast<unsigned long>(esp_random()));
  }
  sessionToken_ = token;
  uploadAccepted_ = false;
  uploadSucceeded_ = false;
  officialPending_ = false;
  officialRunning_ = false;
  rebootPending_ = false;

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(apSsid_.c_str(), apPassword_.c_str())) {
    setStatus("PHONE UPDATE", "Could not start the Book Pet network");
    return false;
  }

  dns_.start(kDnsPort, "*", WiFi.softAPIP());
  if (!routesConfigured_) {
    configureRoutes();
    routesConfigured_ = true;
  }
  server_.begin();
  active_ = true;
  char detail[160];
  snprintf(detail, sizeof(detail), "Join %s, password %s, then open 192.168.4.1",
           apSsid_.c_str(), apPassword_.c_str());
  setStatus("PHONE UPDATE READY", detail);
  Serial.printf("[bookpet] update portal ssid=%s ip=%s\n", apSsid_.c_str(),
                WiFi.softAPIP().toString().c_str());
  return true;
}

void UpdatePortal::configureRoutes() {
  server_.collectHeaders(kCollectedHeaders, 2);
  server_.on("/", HTTP_GET, [this]() {
    server_.send(200, "text/html; charset=utf-8", kBookPetUpdatePortalPage);
  });
  server_.on("/api/status", HTTP_GET, [this]() { sendStatus(); });
  server_.on(
      "/api/upload", HTTP_POST,
      [this]() {
        const int status = uploadSucceeded_ ? 200 : 422;
        const String body =
            String("{\"ok\":") + (uploadSucceeded_ ? "true" : "false") +
            ",\"message\":\"" + jsonEscape(statusDetail_) + "\"}";
        server_.send(status, "application/json", body);
      },
      [this]() { handleUpload(); });
  server_.on("/api/official", HTTP_POST, [this]() {
    if (!originAllowed() || !tokenAllowed()) {
      server_.send(403, "text/plain", "This update request is not authorized.");
      return;
    }
    if (updateBusy()) {
      server_.send(409, "text/plain", "An update is already running.");
      return;
    }
    wifiSsid_ = server_.arg("ssid");
    wifiPassword_ = server_.arg("password");
    wifiSsid_.trim();
    if (wifiSsid_.isEmpty()) {
      server_.send(400, "text/plain", "Enter a Wi-Fi name.");
      return;
    }
    officialPending_ = true;
    server_.send(202, "text/plain",
                 "Book Pet is connecting and checking the official release.");
  });

  const char* captivePaths[] = {"/generate_204", "/hotspot-detect.html",
                                "/ncsi.txt", "/connecttest.txt"};
  for (const char* path : captivePaths) {
    server_.on(path, HTTP_ANY, [this]() {
      server_.sendHeader("Location", "http://192.168.4.1/", true);
      server_.send(302, "text/plain", "");
    });
  }
  server_.onNotFound([this]() {
    server_.sendHeader("Location", "http://192.168.4.1/", true);
    server_.send(302, "text/plain", "");
  });
}

void UpdatePortal::setStatus(const char* title, const char* detail,
                             uint8_t progress) {
  snprintf(statusTitle_, sizeof(statusTitle_), "%s", title ? title : "UPDATE");
  snprintf(statusDetail_, sizeof(statusDetail_), "%s",
           detail ? detail : "");
  progress_ = progress;
  Serial.printf("[bookpet] portal: %s - %s (%u%%)\n", statusTitle_,
                statusDetail_, progress_);
  if (statusCallback_) statusCallback_(statusTitle_, statusDetail_, progress_);
}

void UpdatePortal::sendStatus() {
  const String body =
      String("{\"title\":\"") + jsonEscape(statusTitle_) +
      "\",\"detail\":\"" + jsonEscape(statusDetail_) +
      "\",\"version\":\"" + BOOKPET_VERSION +
      "\",\"signed\":" +
      (FirmwareUpdater::requiresSignature() ? "true" : "false") +
      ",\"progress\":" + String(progress_) +
      ",\"token\":\"" + jsonEscape(sessionToken_.c_str()) + "\"}";
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", body);
}

bool UpdatePortal::originAllowed() const {
  String host = server_.header("Host");
  host.toLowerCase();
  if (host != "192.168.4.1" && host != "192.168.4.1:80") return false;
  const String origin = server_.header("Origin");
  if (origin.isEmpty()) return true;
  return origin == "http://192.168.4.1" ||
         origin == "http://192.168.4.1:80";
}

bool UpdatePortal::tokenAllowed() const {
  return !sessionToken_.isEmpty() &&
         server_.arg("token") == sessionToken_;
}

bool UpdatePortal::updateBusy() const {
  return firmwareUpdater.running() || uploadAccepted_ || officialPending_ ||
         officialRunning_ || rebootPending_;
}

void UpdatePortal::handleUpload() {
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadAccepted_ = false;
    uploadSucceeded_ = false;
    if (updateBusy()) {
      setStatus("UPDATE BLOCKED", "Another update is already running");
      return;
    }
    if (!originAllowed() || !tokenAllowed()) {
      setStatus("UPDATE BLOCKED", "The upload came from another website");
      return;
    }
    const size_t size =
        static_cast<size_t>(server_.arg("size").toDouble());
    const String sha256 = server_.arg("sha256");
    if (size == 0 || size > kMaximumUpdateBytes) {
      setStatus("UPDATE BLOCKED", "The firmware size is not valid");
      return;
    }
    if (!firmwareUpdater.begin(size, sha256.c_str())) {
      setStatus("UPDATE FAILED", firmwareUpdater.error());
      return;
    }
    uploadAccepted_ = true;
    setStatus("INSTALLING UPDATE", "Keep Book Pet powered", 1);
  } else if (upload.status == UPLOAD_FILE_WRITE && uploadAccepted_) {
    if (firmwareUpdater.write(upload.buf, upload.currentSize) !=
        upload.currentSize) {
      firmwareUpdater.abort();
      uploadAccepted_ = false;
      setStatus("UPDATE FAILED", firmwareUpdater.error());
      return;
    }
    progress_ = firmwareUpdater.progressPercent();
  } else if (upload.status == UPLOAD_FILE_END && uploadAccepted_) {
    uploadSucceeded_ = firmwareUpdater.finish();
    uploadAccepted_ = false;
    if (uploadSucceeded_) {
      setStatus("UPDATE VERIFIED", "Restarting into the new version", 100);
      rebootPending_ = true;
      rebootAtMs_ = millis() + 2'500;
    } else {
      setStatus("UPDATE FAILED", firmwareUpdater.error());
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    firmwareUpdater.abort("The browser cancelled the upload");
    uploadAccepted_ = false;
    setStatus("UPDATE CANCELLED", firmwareUpdater.error());
  }
}

bool UpdatePortal::syncSecureClock() {
  configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
  const uint32_t started = millis();
  time_t now = time(nullptr);
  while (now < 1'735'689'600 && millis() - started < kClockTimeoutMs) {
    dns_.processNextRequest();
    server_.handleClient();
    delay(100);
    now = time(nullptr);
  }
  return now >= 1'735'689'600;
}

int UpdatePortal::compareVersions(const char* left, const char* right) {
  if (!left) left = "0";
  if (!right) right = "0";
  for (int part = 0; part < 3; ++part) {
    while (*left == 'v' || *left == '.' || *left == ' ') ++left;
    while (*right == 'v' || *right == '.' || *right == ' ') ++right;
    char* leftEnd = nullptr;
    char* rightEnd = nullptr;
    const long l = strtol(left, &leftEnd, 10);
    const long r = strtol(right, &rightEnd, 10);
    left = leftEnd;
    right = rightEnd;
    if (l != r) return l > r ? 1 : -1;
  }
  return 0;
}

void UpdatePortal::performOfficialUpdateWork() {
  setStatus("CONNECTING TO WI-FI", wifiSsid_.c_str(), 0);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid_.c_str(), wifiPassword_.c_str());
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < kWifiTimeoutMs) {
    dns_.processNextRequest();
    server_.handleClient();
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    setStatus("WI-FI FAILED",
              "Check the network name and password, then try again");
    return;
  }

  setStatus("SECURING CONNECTION", "Setting the clock for HTTPS");
  if (!syncSecureClock()) {
    setStatus("ONLINE UPDATE FAILED", "Could not set a secure network clock");
    WiFi.disconnect(false);
    return;
  }

  setStatus("CHECKING OFFICIAL RELEASE", "Reading the stable update manifest");
  NetworkClientSecure manifestClient;
  manifestClient.setCACert(kBookPetUpdateRootCa);
  HTTPClient manifestHttp;
  manifestHttp.setConnectTimeout(15'000);
  manifestHttp.setTimeout(20'000);
  manifestHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!manifestHttp.begin(manifestClient, BOOKPET_UPDATE_MANIFEST_URL)) {
    setStatus("ONLINE UPDATE FAILED", "Could not open the update site");
    return;
  }
  manifestHttp.addHeader("User-Agent",
                         String("BookPet/") + BOOKPET_VERSION);
  const int manifestCode = manifestHttp.GET();
  if (manifestCode != HTTP_CODE_OK) {
    char detail[96];
    snprintf(detail, sizeof(detail), "Update site returned HTTP %d",
             manifestCode);
    manifestHttp.end();
    setStatus("ONLINE UPDATE FAILED", detail);
    return;
  }

  JsonDocument manifest;
  const DeserializationError jsonError =
      deserializeJson(manifest, manifestHttp.getStream());
  manifestHttp.end();
  if (jsonError) {
    setStatus("ONLINE UPDATE FAILED", "The update manifest is not valid");
    return;
  }
  const char* product = manifest["product"] | "";
  const char* version = manifest["version"] | "";
  const char* url = manifest["url"] | "";
  const char* sha256 = manifest["sha256"] | "";
  const size_t size = manifest["size"] | 0;
  if (strcmp(product, kProductId) != 0 || strlen(version) == 0 ||
      strncmp(url, kAllowedUpdatePrefix, strlen(kAllowedUpdatePrefix)) != 0 ||
      strlen(sha256) != 64 || size < 32 * 1024 ||
      size > kMaximumUpdateBytes) {
    setStatus("ONLINE UPDATE FAILED", "The update manifest failed validation");
    return;
  }
  if (compareVersions(version, BOOKPET_VERSION) <= 0) {
    char detail[96];
    snprintf(detail, sizeof(detail), "Version %s is already current", version);
    setStatus("BOOK PET IS UP TO DATE", detail, 100);
    return;
  }

  char detail[96];
  snprintf(detail, sizeof(detail), "Downloading Book Pet %s", version);
  setStatus("INSTALLING OFFICIAL UPDATE", detail, 1);
  NetworkClientSecure binaryClient;
  binaryClient.setCACert(kBookPetUpdateRootCa);
  HTTPClient binaryHttp;
  binaryHttp.setConnectTimeout(15'000);
  binaryHttp.setTimeout(30'000);
  binaryHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!binaryHttp.begin(binaryClient, url)) {
    setStatus("ONLINE UPDATE FAILED", "Could not open the firmware download");
    return;
  }
  binaryHttp.addHeader("User-Agent", String("BookPet/") + BOOKPET_VERSION);
  const int binaryCode = binaryHttp.GET();
  if (binaryCode != HTTP_CODE_OK) {
    snprintf(detail, sizeof(detail), "Firmware download returned HTTP %d",
             binaryCode);
    binaryHttp.end();
    setStatus("ONLINE UPDATE FAILED", detail);
    return;
  }
  const int contentLength = binaryHttp.getSize();
  if (contentLength > 0 && static_cast<size_t>(contentLength) != size) {
    binaryHttp.end();
    setStatus("ONLINE UPDATE FAILED", "Firmware size did not match manifest");
    return;
  }

  const bool installed =
      firmwareUpdater.install(binaryHttp.getStream(), size, sha256);
  binaryHttp.end();
  if (!installed) {
    setStatus("ONLINE UPDATE FAILED", firmwareUpdater.error());
    return;
  }
  setStatus("UPDATE VERIFIED", "Restarting into the new version", 100);
  rebootPending_ = true;
  rebootAtMs_ = millis() + 2'500;
}

void UpdatePortal::performOfficialUpdate() {
  officialPending_ = false;
  officialRunning_ = true;
  performOfficialUpdateWork();
  wifiSsid_ = "";
  wifiPassword_ = "";
  WiFi.disconnect(false);
  WiFi.setSleep(true);
  officialRunning_ = false;
}

void UpdatePortal::handle() {
  if (!active_) return;
  dns_.processNextRequest();
  server_.handleClient();
  if (officialPending_) performOfficialUpdate();
  if (rebootPending_ && static_cast<int32_t>(millis() - rebootAtMs_) >= 0) {
    delay(100);
    ESP.restart();
  }
}

bool UpdatePortal::safeToStop() const {
  return !updateBusy();
}

void UpdatePortal::stop() {
  if (!active_) return;
  firmwareUpdater.abort();
  server_.stop();
  dns_.stop();
  WiFi.disconnect(true, false);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  active_ = false;
  officialPending_ = false;
  officialRunning_ = false;
  rebootPending_ = false;
  sessionToken_ = "";
  wifiSsid_ = "";
  wifiPassword_ = "";
  statusCallback_ = nullptr;
  setStatus("PHONE UPDATE", "Stopped");
}

}  // namespace bookpet
