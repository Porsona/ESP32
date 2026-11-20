#include "NetTypes.h"
#include <stdio.h>
#include <string.h>

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
bool httpsUrlHost(const char* url,char* out,size_t size) {
  static const char PREFIX[]="https://";
  if (!url || !out || size<2 || strncmp(url,PREFIX,sizeof(PREFIX)-1)) return false;
  const char* host=url+sizeof(PREFIX)-1;
  size_t n=0;
  while (host[n] && host[n]!='/' && host[n]!='?' && host[n]!='#') {
    const char c=host[n];
    const bool ok=(c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='.';
    if (!ok || n+1>=size) return false;  // ':' (explicit port), '@' (userinfo) or too long.
    out[n]=c; ++n;
  }
  if (!n) return false;
  out[n]=0;
  return true;
}
}  // namespace deskos
