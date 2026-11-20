#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Level L starts at 50*L*(L-1) XP: each level needs 100*L more XP.
// L2=100, L3=300, L5=1000, L7=2100, L10=4500. The pet never grants XP.
constexpr uint8_t PET_MAX_LEVEL=99;
uint8_t petLevel(uint32_t totalXP);
uint32_t levelStartXP(uint8_t level);
uint16_t levelPermille(uint32_t totalXP);

enum class PetStyle : uint8_t { Classic, StarEyes, HeartEyes, Crown, Aura, Count };
constexpr uint8_t PET_STYLE_COUNT=static_cast<uint8_t>(PetStyle::Count);
uint8_t styleUnlockLevel(PetStyle style);
inline bool styleUnlocked(PetStyle style,uint8_t level) { return level>=styleUnlockLevel(style); }
const char* styleName(PetStyle style);
// Style unlocked by reaching `level` exactly, or Count when none.
PetStyle styleUnlockedAt(uint8_t level);

enum class PetMood : uint8_t { Idle, Happy, Sleep, Celebrate };
const char* moodName(PetMood mood);

constexpr uint16_t PET_SCHEMA_VERSION=1;
struct PetRecord {
  uint32_t magic;
  uint16_t version,bytes;
  uint32_t lastSeenXP;   // Shared XP when the pet last saw the player.
  uint32_t petCount;
  uint8_t style,lastSeenLevel,reserved[2];
  uint32_t checksum;
};
static_assert(sizeof(PetRecord)==24,"Change pet schema version when the record layout changes");
PetRecord defaultPetRecord();
void sealPetRecord(PetRecord& record);
bool validPetRecord(const PetRecord& record);
}  // namespace deskos
