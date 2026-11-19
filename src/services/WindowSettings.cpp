#include "WindowSettings.h"
#include <string.h>
#include "Crc32.h"

namespace deskos {
namespace {
constexpr uint32_t MAGIC=0x57494E31UL;  // "WIN1"
const char* const NS="deskos-win";
const char* const KEY="state";
}
WindowSettingsRecord defaultWindowSettings() {
  WindowSettingsRecord r;
  memset(&r,0,sizeof(r));
  r.magic=MAGIC; r.version=WINDOW_SCHEMA_VERSION; r.bytes=sizeof(r); r.scene=0; r.speed=1;
  sealWindowSettings(r);
  return r;
}
void sealWindowSettings(WindowSettingsRecord& r) { r.checksum=crc32Bytes(&r,offsetof(WindowSettingsRecord,checksum)); }
bool validWindowSettings(const WindowSettingsRecord& r) {
  return r.magic==MAGIC && r.version==WINDOW_SCHEMA_VERSION && r.bytes==sizeof(r) && r.scene<4 && r.speed<3 &&
         !r.reserved[0] && !r.reserved[1] && r.checksum==crc32Bytes(&r,offsetof(WindowSettingsRecord,checksum));
}
BlobLoad loadWindowSettings(IBlobStore& store,WindowSettingsRecord& out) {
  WindowSettingsRecord candidate=defaultWindowSettings();
  BlobLoad result=store.load(NS,KEY,&candidate,sizeof(candidate));
  if (result==BlobLoad::Loaded && !validWindowSettings(candidate)) result=BlobLoad::Invalid;
  out=result==BlobLoad::Loaded?candidate:defaultWindowSettings();
  return result;
}
bool saveWindowSettings(IBlobStore& store,const WindowSettingsRecord& record) {
  return validWindowSettings(record) && store.save(NS,KEY,&record,sizeof(record));
}
}  // namespace deskos
