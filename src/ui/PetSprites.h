#pragma once
#include <stdint.h>
#include "../services/PetModel.h"

namespace deskos {
// "BIT": original 16x16 sprout-slime. Rows 0-3 head, 4-15 body, eyes on
// rows 8-9 and mouth on rows 11-12 are swapped per mood/style. Flash only.
constexpr uint8_t PET_SPRITE_SIZE=16;
enum class PetHead : uint8_t { LeafUp, LeafDown, Crown };
enum class PetEyes : uint8_t { Open, Closed, Happy, Sleepy, Star, Heart };
enum class PetMouth : uint8_t { Smile, Open, Small };
void composePet(const char* rows[PET_SPRITE_SIZE],PetHead head,PetEyes eyes,PetMouth mouth);
// Frame selection is pure so it can be tested without a display.
struct PetFrame { PetHead head; PetEyes eyes; PetMouth mouth; };
PetFrame petFrame(PetMood mood,PetStyle style,uint32_t moodElapsedMs,uint32_t animMs);
// Exposed for tests: every sprite string must be exactly 16 characters.
bool petSpritesWellFormed();
}  // namespace deskos
