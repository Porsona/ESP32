#include "PetSprites.h"
#include <string.h>

namespace deskos {
namespace {
const char* const HEADS[3][4]={
  {"........###.....",
   ".......#+++#....",
   "........###.....",
   "........#......."},
  {"................",
   "........###.....",
   ".......#+++#....",
   "........##......"},
  {"................",
   ".....g.gg.g.....",
   ".....gggggg.....",
   ".....g*gg*g....."}
};
const char* const BODY[12]={  // Rows 4..15; rows 8,9,11,12 replaced by face parts.
  ".....######.....",
  "...##oooooo##...",
  "..#oooooooooo#..",
  ".#oooooooooooo#.",
  nullptr,nullptr,
  "#oooooooooooooo#",
  nullptr,nullptr,
  "#oooooooooooooo#",
  ".#++++++++++++#.",
  "..############.."
};
const char* const EYES[6][2]={
  {".#oow#oooow#oo#.",".#oo##oooo##oo#."},  // Open, with highlight.
  {".#oooooooooooo#.",".#oo##oooo##oo#."},  // Closed (blink).
  {".#ooo#oooo#ooo#.",".#oo#o#oo#o#oo#."},  // Happy ^ ^.
  {".#oooooooooooo#.",".#o###oooo###o#."},  // Sleepy.
  {".#oo*oooooo*oo#.",".#o***oooo***o#."},  // Star (L3).
  {".#o*o*oooo*o*o#.",".#oo*oooooo*oo#."}   // Heart (L5).
};
const char* const MOUTHS[3][2]={
  {"#o**o#oooo#o**o#","#ooooo####ooooo#"},  // Smile.
  {"#o**oo####oo**o#","#ooooo#**#ooooo#"},  // Open, tongue.
  {"#o**oooooooo**o#","#oooooo##oooooo#"}   // Small (sleep).
};
}  // namespace

void composePet(const char* rows[PET_SPRITE_SIZE],PetHead head,PetEyes eyes,PetMouth mouth) {
  const uint8_t h=static_cast<uint8_t>(head)<3?static_cast<uint8_t>(head):0;
  const uint8_t e=static_cast<uint8_t>(eyes)<6?static_cast<uint8_t>(eyes):0;
  const uint8_t m=static_cast<uint8_t>(mouth)<3?static_cast<uint8_t>(mouth):0;
  for (uint8_t i=0;i<4;++i) rows[i]=HEADS[h][i];
  for (uint8_t i=0;i<12;++i) rows[4+i]=BODY[i];
  rows[8]=EYES[e][0]; rows[9]=EYES[e][1];
  rows[11]=MOUTHS[m][0]; rows[12]=MOUTHS[m][1];
}

PetFrame petFrame(PetMood mood,PetStyle style,uint32_t moodElapsedMs,uint32_t animMs) {
  PetFrame f;
  const bool crown=style==PetStyle::Crown || style==PetStyle::Aura;
  const PetEyes styled=style==PetStyle::StarEyes?PetEyes::Star:style==PetStyle::HeartEyes?PetEyes::Heart:PetEyes::Open;
  switch (mood) {
    case PetMood::Sleep:
      f.head=(animMs/900)%2?PetHead::LeafDown:PetHead::LeafUp;
      f.eyes=PetEyes::Sleepy; f.mouth=PetMouth::Small; break;
    case PetMood::Happy:
    case PetMood::Celebrate:
      f.head=(animMs/250)%2?PetHead::LeafDown:PetHead::LeafUp;
      f.eyes=styled==PetEyes::Open?PetEyes::Happy:styled; f.mouth=PetMouth::Open; break;
    default: {
      f.head=(animMs/600)%2?PetHead::LeafDown:PetHead::LeafUp;
      const bool blink=(moodElapsedMs%3200)>=3050;  // 150 ms blink every 3.2 s.
      f.eyes=blink?PetEyes::Closed:styled; f.mouth=PetMouth::Smile; break;
    }
  }
  if (crown) f.head=PetHead::Crown;
  return f;
}

bool petSpritesWellFormed() {
  for (const auto& head:HEADS) for (const char* row:head) if (strlen(row)!=16) return false;
  for (const char* row:BODY) if (row && strlen(row)!=16) return false;
  for (const auto& e:EYES) for (const char* row:e) if (strlen(row)!=16) return false;
  for (const auto& m:MOUTHS) for (const char* row:m) if (strlen(row)!=16) return false;
  return true;
}
}  // namespace deskos
