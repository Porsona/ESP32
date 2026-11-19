#include "NetworkService.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cstring>

namespace deskos {
namespace {
const char* const NS="deskos-net";
constexpr uint32_t RETRY_MIN_MS=10000,RETRY_MAX_MS=120000;
}

size_t NetworkService::passwordLength() const { return std::strlen(pass_); }

void NetworkService::begin() {
  Preferences prefs;
  if (prefs.begin(NS,true)) {  // Read-only; a missing namespace simply means "not configured".
    prefs.getString("ssid",ssid_,sizeof(ssid_));
    prefs.getString("pass",pass_,sizeof(pass_));
    enabled_=prefs.getUChar("en",1)!=0;
    prefs.end();
  }
  ssid_[sizeof(ssid_)-1]=pass_[sizeof(pass_)-1]=0;
  state_=!ssid_[0]?NetState::NoConfig:enabled_?NetState::Connecting:NetState::Off;
  if (ssid_[0] && enabled_) startStation(millis());
}

bool NetworkService::persist() {
  Preferences prefs;
  if (!prefs.begin(NS,false)) return false;
  const bool ok=prefs.putString("ssid",ssid_)==std::strlen(ssid_) &&
                prefs.putString("pass",pass_)==std::strlen(pass_) && prefs.putUChar("en",enabled_?1:0)==1;
  prefs.end();
  return ok;
}

void NetworkService::startStation(uint32_t nowMs) {
  if (!stationOn_) {
    WiFi.persistent(false);          // The SDK must not keep its own credential copy in flash.
    WiFi.setHostname("deskos-cyd");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    stationOn_=true;
  }
  WiFi.begin(ssid_,pass_[0]?pass_:nullptr);  // Returns immediately; status polled in update().
  lastAttempt_=nowMs; state_=NetState::Connecting;
  Serial.printf("NET: CONNECTING ssid=\"%s\" pass_len=%u\n",ssid_,static_cast<unsigned>(passwordLength()));
}

void NetworkService::stopStation() {
  if (stationOn_) { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); stationOn_=false; }
  ip_=mask_=0; rssi_=0;
}

void NetworkService::update(uint32_t nowMs) {
  if (!stationOn_) return;
  const wl_status_t status=WiFi.status();
  if (status==WL_CONNECTED) {
    if (state_!=NetState::Connected) {
      state_=NetState::Connected; retryDelay_=RETRY_MIN_MS;
      ip_=static_cast<uint32_t>(WiFi.localIP()); mask_=static_cast<uint32_t>(WiFi.subnetMask());
      rssi_=static_cast<int8_t>(WiFi.RSSI()); lastRssiAt_=nowMs;
      Serial.printf("NET: CONNECTED ip=%s rssi=%d\n",WiFi.localIP().toString().c_str(),static_cast<int>(rssi_));
    } else if (nowMs-lastRssiAt_>=5000) { rssi_=static_cast<int8_t>(WiFi.RSSI()); lastRssiAt_=nowMs; }
    return;
  }
  if (state_==NetState::Connected) {
    state_=NetState::Connecting; ip_=mask_=0; lastAttempt_=nowMs; ++reconnects_;
    Serial.printf("NET: DISCONNECTED status=%d (auto-reconnect)\n",static_cast<int>(status));
    return;
  }
  // Still not connected: re-issue begin() with exponential backoff (10 s .. 2 min).
  if (nowMs-lastAttempt_>=retryDelay_) {
    retryDelay_=retryDelay_*2>RETRY_MAX_MS?RETRY_MAX_MS:retryDelay_*2;
    WiFi.disconnect(false);
    startStation(nowMs);
  }
}

bool NetworkService::setSsid(const char* ssid) {
  const size_t n=ssid?std::strlen(ssid):0;
  if (n==0 || n>32) return false;
  std::memset(ssid_,0,sizeof(ssid_)); std::memcpy(ssid_,ssid,n);
  if (!persist()) return false;
  if (state_==NetState::NoConfig) state_=enabled_?NetState::Connecting:NetState::Off;
  if (enabled_) { retryDelay_=RETRY_MIN_MS; startStation(millis()); }
  return true;
}
bool NetworkService::setPassword(const char* password) {
  const size_t n=password?std::strlen(password):0;
  if (n>64 || (n>0 && n<8)) return false;  // WPA2 passphrase is 8..63 chars (64 = hex PSK).
  std::memset(pass_,0,sizeof(pass_)); if (n) std::memcpy(pass_,password,n);
  if (!persist()) return false;
  if (enabled_ && ssid_[0]) { retryDelay_=RETRY_MIN_MS; startStation(millis()); }
  return true;
}
void NetworkService::setEnabled(bool enabled) {
  enabled_=enabled; persist();
  if (!ssid_[0]) { state_=NetState::NoConfig; return; }
  if (enabled) { retryDelay_=RETRY_MIN_MS; startStation(millis()); }
  else { stopStation(); state_=NetState::Off; }
}
void NetworkService::forget() {
  stopStation();
  std::memset(ssid_,0,sizeof(ssid_)); std::memset(pass_,0,sizeof(pass_));
  Preferences prefs;
  if (prefs.begin(NS,false)) { prefs.remove("ssid"); prefs.remove("pass"); prefs.end(); }  // Only this namespace.
  state_=NetState::NoConfig;
}
}  // namespace deskos
