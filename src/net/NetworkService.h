#pragma once
#include "../services/NetTypes.h"

namespace deskos {
// Wi-Fi station manager. Credentials live only in NVS ("deskos-net"), entered
// over Serial; never in source or Git. All calls are non-blocking; the Wi-Fi
// driver is not even initialised until credentials exist and Wi-Fi is enabled.
class NetworkService : public INetworkStatus {
 public:
  void begin();
  void update(uint32_t nowMs);
  bool setSsid(const char* ssid);
  bool setPassword(const char* password);   // Empty string = open network.
  void setEnabled(bool enabled);
  void forget();                            // Removes stored credentials.
  NetState state() const override { return state_; }
  uint32_t ipv4() const override { return ip_; }
  uint32_t subnetMask() const override { return mask_; }
  int8_t rssi() const override { return rssi_; }
  const char* ssid() const { return ssid_; }
  size_t passwordLength() const;
  bool enabled() const { return enabled_; }
  uint32_t reconnects() const { return reconnects_; }
 private:
  void startStation(uint32_t nowMs);
  void stopStation();
  bool persist();
  char ssid_[33]={0};
  char pass_[65]={0};
  bool enabled_=true,stationOn_=false;
  NetState state_=NetState::NoConfig;
  uint32_t ip_=0,mask_=0,lastAttempt_=0,retryDelay_=10000,reconnects_=0,lastRssiAt_=0;
  int8_t rssi_=0;
};
}  // namespace deskos
