#include "NetTypes.h"
#include <stdio.h>

namespace deskos {
const char* netStateName(NetState s) {
  switch (s) {
    case NetState::NoConfig: return "NO CONFIG";
    case NetState::Connecting: return "CONNECTING";
    case NetState::Connected: return "CONNECTED";
    default: return "OFF";
  }
}
void formatIPv4(uint32_t a,char* out,size_t size) {
  snprintf(out,size,"%u.%u.%u.%u",static_cast<unsigned>(a&0xFF),static_cast<unsigned>((a>>8)&0xFF),
           static_cast<unsigned>((a>>16)&0xFF),static_cast<unsigned>((a>>24)&0xFF));
}
}  // namespace deskos
