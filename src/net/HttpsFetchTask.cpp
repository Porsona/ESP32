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
// Priority 0 = IDLE priority: the task time-slices with IDLE0 instead of
// starving it during TLS crypto (this framework enables TASK_WDT_PANIC with
// a 5 s timeout and watches IDLE0). Wi-Fi/lwIP tasks keep their priority.
constexpr UBaseType_t TASK_PRIORITY=tskIDLE_PRIORITY;

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

CivilTime civil(const mbedtls_x509_time& t) { return CivilTime{t.year,t.mon,t.day,t.hour,t.min,t.sec}; }
}  // namespace

bool HttpsFetchTask::begin() {
  if (task_) return true;
  // Core 0 next to the Wi-Fi stack; the Arduino loop (UI) stays on core 1.
  return xTaskCreatePinnedToCore(entry,"deskos-https",STACK_BYTES,this,TASK_PRIORITY,&task_,0)==pdPASS;
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
    const char* certNote="NOT_CONNECTED";
    {
      const uint32_t started=millis();
      char host[HOST_MAX];
      if (!httpsUrlHost(url_,host,sizeof(host))) code=ERROR_BAD_URL;
      // 1) DNS + TCP + TLS handshake (CA chain + hostname) on our own client.
      else if (!client.connect(host,443,CONNECT_TIMEOUT_MS)) {
        char reason[64]={0};
        const int tlsError=client.lastError(reason,sizeof(reason));
        Serial.printf("HTTPS: CONNECT_FAILED host=%s tls_error=%d %s\n",host,tlsError,reason);
        code=ERROR_CONNECT;
      } else {
        // 2) Validity dates of the leaf certificate, only with trustworthy time.
        const mbedtls_x509_crt* leaf=client.getPeerCertificate();
        bool dateOk=true;
        if (!leaf) { certNote="NO_PEER_CERT"; dateOk=false; code=ERROR_NO_PEER_CERT; }
        else if (!time_.valid()) certNote="DATE_UNCHECKED_NO_NTP";
        else {
          const CertDate d=certDateStatus(civil(leaf->valid_from),civil(leaf->valid_to),time_.epoch());
          certNote=certDateName(d);
          if (d!=CertDate::Valid) { dateOk=false; code=ERROR_CERT_DATE; }
        }
        // 3) HTTP/1.0 GET over the already-verified connection (HTTPClient reuses it).
        if (dateOk) {
          client.setTimeout((READ_TIMEOUT_MS+500)/1000);
          HTTPClient http;
          http.setConnectTimeout(CONNECT_TIMEOUT_MS);
          http.setTimeout(READ_TIMEOUT_MS);
          http.setReuse(false);
          http.useHTTP10(true);  // No keep-alive; body ends at connection close.
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
        }
      }
      client.stop();
      body_[length]=0;
      Serial.printf("HTTPS: code=%d bytes=%u ms=%lu cert_date=%s stack_free=%lu heap=%u max_block=%u min_heap=%u\n",
                    code,static_cast<unsigned>(length),static_cast<unsigned long>(millis()-started),certNote,
                    static_cast<unsigned long>(stackHighWaterBytes()),ESP.getFreeHeap(),ESP.getMaxAllocHeap(),
                    ESP.getMinFreeHeap());
    }
    portENTER_CRITICAL(&lock_);
    code_=code; length_=length; state_=FetchState::Done;
    portEXIT_CRITICAL(&lock_);
  }
}
}  // namespace deskos
