#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../services/NetTypes.h"
#include "../services/TimeModel.h"

namespace deskos {
// One long-lived FreeRTOS task (core 0) performs blocking HTTPS GETs so the UI
// loop never waits on DNS/TLS/socket timeouts. Fixed buffers, no per-request
// task creation. TLS verifies the server chain + hostname against
// ROOT_CA_BUNDLE; the leaf certificate's validity dates are checked here
// against NTP time (mbedTLS in this framework build cannot do it).
class HttpsFetchTask : public IHttpFetcher {
 public:
  static constexpr size_t URL_MAX=256,BODY_MAX=2048,HOST_MAX=64;
  static constexpr uint32_t STACK_BYTES=12288;
  // Negative transport codes besides HTTPClient's own (-1..-11).
  static constexpr int ERROR_TOO_LARGE=-100,ERROR_CERT_DATE=-101,ERROR_NO_PEER_CERT=-102,
                       ERROR_BAD_URL=-103,ERROR_CONNECT=-104;
  explicit HttpsFetchTask(const ITimeSource& time) : time_(time) {}
  bool begin();
  bool start(uint32_t requestId,const char* url) override;
  FetchState poll(uint32_t requestId,FetchResult& out) override;
  uint32_t stackHighWaterBytes() const;
 private:
  static void entry(void* self);
  void run();
  const ITimeSource& time_;
  portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t task_=nullptr;
  FetchState state_=FetchState::Idle;
  uint32_t id_=0;
  int code_=0;
  size_t length_=0;
  char url_[URL_MAX]={0};
  char body_[BODY_MAX+1]={0};
};
}  // namespace deskos
