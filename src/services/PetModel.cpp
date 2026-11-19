#include "PetModel.h"
#include <string.h>
#include "Crc32.h"

namespace deskos {
namespace {
constexpr uint32_t MAGIC=0x50455431UL;  // "PET1"
const uint8_t UNLOCK[PET_STYLE_COUNT]={1,3,5,7,10};
const char* const STYLE_NAMES[PET_STYLE_COUNT]={"CLASSIC","STAR EYES","HEART EYES","CROWN","AURA"};
}

uint32_t levelStartXP(uint8_t level) {
  if (level<=1) return 0;
  if (level>PET_MAX_LEVEL) level=PET_MAX_LEVEL;
  return 50UL*level*(level-1U);  // Max 485,100: no overflow.
}
uint8_t petLevel(uint32_t xp) {
  uint8_t level=1;
  while (level<PET_MAX_LEVEL && xp>=levelStartXP(static_cast<uint8_t>(level+1))) ++level;
  return level;
}
uint16_t levelPermille(uint32_t xp) {
  const uint8_t level=petLevel(xp);
  if (level>=PET_MAX_LEVEL) return 1000;
  const uint32_t start=levelStartXP(level),span=levelStartXP(static_cast<uint8_t>(level+1))-start;
  return static_cast<uint16_t>(static_cast<uint64_t>(xp-start)*1000/span);
}
uint8_t styleUnlockLevel(PetStyle s) {
  const uint8_t i=static_cast<uint8_t>(s);
  return i<PET_STYLE_COUNT?UNLOCK[i]:255;
}
const char* styleName(PetStyle s) {
  const uint8_t i=static_cast<uint8_t>(s);
  return i<PET_STYLE_COUNT?STYLE_NAMES[i]:"?";
}
PetStyle styleUnlockedAt(uint8_t level) {
  for (uint8_t i=1;i<PET_STYLE_COUNT;++i) if (UNLOCK[i]==level) return static_cast<PetStyle>(i);
  return PetStyle::Count;
}
const char* moodName(PetMood m) {
  switch (m) {
    case PetMood::Happy: return "HAPPY";
    case PetMood::Sleep: return "SLEEP";
    case PetMood::Celebrate: return "LEVEL UP";
    default: return "IDLE";
  }
}

PetRecord defaultPetRecord() {
  PetRecord r;
  memset(&r,0,sizeof(r));
  r.magic=MAGIC; r.version=PET_SCHEMA_VERSION; r.bytes=sizeof(r); r.lastSeenLevel=1;
  sealPetRecord(r);
  return r;
}
void sealPetRecord(PetRecord& r) { r.checksum=crc32Bytes(&r,offsetof(PetRecord,checksum)); }
bool validPetRecord(const PetRecord& r) {
  return r.magic==MAGIC && r.version==PET_SCHEMA_VERSION && r.bytes==sizeof(r) &&
         r.style<PET_STYLE_COUNT && r.lastSeenLevel>=1 && r.lastSeenLevel<=PET_MAX_LEVEL &&
         !r.reserved[0] && !r.reserved[1] && r.checksum==crc32Bytes(&r,offsetof(PetRecord,checksum));
}
}  // namespace deskos
