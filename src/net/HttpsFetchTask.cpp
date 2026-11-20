#include "HttpsFetchTask.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include "RootCertificates.h"

namespace deskos {
namespace {
constexpr int CONNECT_TIMEOUT_MS=6000;
constexpr uint16_t READ_TIMEOUT_MS=8000;
constexpr unsigned long HANDSHAKE_TIMEOUT_S=12;
constexpr int ERROR_TOO_LARGE=-100;

// Fixed-size sink for HTTPClient::writeToStream (handles chunked bodies; no String).
class BufferSink : public Stream {
 public:
  BufferSink(char* buffer,size_t capacity) : buffer_(buffer),capacity_(capacity) {}
  size_t write(uint8_t c) override { return write(&c,1); }
  size_t write(const uint8_t* data,size_t size) override {
    if (length_+size>capacity_) { overflow_=true; return 0; }
    std::memcpy(buffer_+length_,data,size); length_+=size; return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t length() const { return length_; }
  bool overflow() const { return overflow_; }
 private:
  char* buffer_;
  size_t capacity_,length_=0;
  bool overflow_=false;
};
}  // namespace

bool HttpsFetchTask::begin() {
  if (task_) return true;
  // Core 0 next to the Wi-Fi stack; the Arduino loop (UI) stays on core 1.
  return xTaskCreatePinnedToCore(entry,"deskos-https",STACK_BYTES,this,1,&task_,0)==pdPASS;
}

bool HttpsFetchTask::start(uint32_t requestId,const char* url) {
  if (!task_ || !url || std::strlen(url)>=URL_MAX) return false;
  portENTER_CRITICAL(&lock_);
  const bool free=state_!=FetchState::Busy;  // A finished, unread result may be replaced.
  if (free) { state_=FetchState::Busy; id_=requestId; std::strcpy(url_,url); }
  portEXIT_CRITICAL(&lock_);
  if (free) xTaskNotifyGive(task_);
  return free;
}

FetchState HttpsFetchTask::poll(uint32_t requestId,FetchResult& out) {
  portENTER_CRITICAL(&lock_);
  FetchState result=state_;
  if (state_==FetchState::Done) {
    if (id_==requestId) { out.httpCode=code_; out.length=length_; out.body=body_; state_=FetchState::Idle; }
    else result=FetchState::Idle;  // Stale result of an abandoned request.
  } else if (state_==FetchState::Busy && id_!=requestId) result=FetchState::Busy;
  portEXIT_CRITICAL(&lock_);
  return result;
}

uint32_t HttpsFetchTask::stackHighWaterBytes() const {
  return task_?uxTaskGetStackHighWaterMark(task_)*sizeof(StackType_t):0;
}

void HttpsFetchTask::entry(void* self) { static_cast<HttpsFetchTask*>(self)->run(); }

void HttpsFetchTask::run() {
  static WiFiClientSecure client;  // Constructed once; the TLS context is freed by stop().
  client.setCACert(ROOT_CA_BUNDLE);  // Chain + hostname verification stay enabled.
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT_S);
  for (;;) {
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    int code=-1;
    size_t length=0;
    {
      HTTPClient http;
      http.setConnectTimeout(CONNECT_TIMEOUT_MS);
      http.setTimeout(READ_TIMEOUT_MS);
      http.setReuse(false);
      http.useHTTP10(true);  // No keep-alive; body ends at connection close.
      const uint32_t started=millis();
      if (http.begin(client,url_)) {
        code=http.GET();
        if (code==HTTP_CODE_OK) {
          BufferSink sink(body_,BODY_MAX);
          const int written=http.writeToStream(&sink);
          length=sink.length();
          if (sink.overflow()) code=ERROR_TOO_LARGE;
          else if (written<0) code=written;
        }
        http.end();
      }
      client.stop();
      body_[length]=0;
      Serial.printf("HTTPS: code=%d bytes=%u ms=%lu stack_free=%lu heap=%u\n",code,static_cast<unsigned>(length),
                    static_cast<unsigned long>(millis()-started),static_cast<unsigned long>(stackHighWaterBytes()),
                    ESP.getFreeHeap());
    }
    portENTER_CRITICAL(&lock_);
    code_=code; length_=length; state_=FetchState::Done;
    portEXIT_CRITICAL(&lock_);
  }
}
}  // namespace deskos
