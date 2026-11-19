#pragma once
#include <stdint.h>
extern uint32_t testNow;
inline uint32_t millis() { return testNow; }
inline uint32_t micros() { return testNow*1000; }
struct TestSerial { template<typename... Args> void printf(const char*,Args...) {} };
struct TestESP { unsigned getFreeHeap() const { return 200000; } unsigned getMaxAllocHeap() const { return 100000; } };
extern TestSerial Serial;
extern TestESP ESP;
