#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
enum class NetState : uint8_t { Off, NoConfig, Connecting, Connected };
const char* netStateName(NetState state);

class INetworkStatus {
 public:
  virtual ~INetworkStatus() = default;
  virtual NetState state() const = 0;
  bool connected() const { return state()==NetState::Connected; }
  virtual uint32_t ipv4() const = 0;        // Network byte order as returned by lwIP (a.b.c.d -> d<<24|...|a).
  virtual uint32_t subnetMask() const = 0;
  virtual int8_t rssi() const = 0;
};
// "a.b.c.d" for an lwIP-order address.
void formatIPv4(uint32_t address,char* out,size_t size);
// Host part of "https://<host>[:port|/|?...]" (port 443 only). False for any
// other scheme, an explicit port, an empty host or one that does not fit.
bool httpsUrlHost(const char* url,char* out,size_t size);

// Asynchronous HTTP(S) GET. start() returns false while a request is running.
enum class FetchState : uint8_t { Idle, Busy, Done };
struct FetchResult { int httpCode; size_t length; const char* body; };  // httpCode<=0: transport error.
class IHttpFetcher {
 public:
  virtual ~IHttpFetcher() = default;
  virtual bool start(uint32_t requestId,const char* url) = 0;
  // Done is returned once per finished request with the matching id.
  virtual FetchState poll(uint32_t requestId,FetchResult& out) = 0;
};
}  // namespace deskos
