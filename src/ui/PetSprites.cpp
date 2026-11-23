#include "PetSprites.h"
#include <string.h>
#include "PetSpriteData.h"

namespace deskos {
namespace {
constexpr uint8_t POSE_COUNT=static_cast<uint8_t>(PetPose::Count);
constexpr uint8_t EYE_COUNT=static_cast<uint8_t>(PetEyes::Count);
constexpr uint8_t STORED_POSES=static_cast<uint8_t>(PetPose::Hang);  // Hang/HangSwing are built from Cheer.
static_assert(petart::SIZE==PET_SPRITE_SIZE && petart::POSES==STORED_POSES && petart::EYES==EYE_COUNT,
              "Generated pet art does not match the pose/lens enums");
static_assert(POSE_COUNT*16<=256 && EYE_COUNT*2<=16,"Pet frame key would overflow");
static_assert(PET_HANG_SWING<PET_SPRITE_SIZE/4,"Swing too wide");

// '.' is transparent in overlays, so they only paint their own cells.
void stamp(PetCanvas& out,const char* row,uint8_t width,int16_t x,int16_t y) {
  for (uint8_t i=0;i<width;++i) if (row[i]!='.') out.rows[y][x+i]=row[i];
}
void composeStored(PetCanvas& out,uint8_t p,uint8_t e,bool crown) {
  memcpy(out.rows,petart::FRAMES[p],sizeof(out.rows));
  const int8_t dx=petart::HEAD_OFFSET[p][0],dy=petart::HEAD_OFFSET[p][1];
  for (uint8_t y=0;y<petart::EYE_H;++y)
    stamp(out,petart::LENSES[e][y],petart::EYE_W,petart::EYE_X+dx,petart::EYE_Y+dy+y);
  if (crown) for (uint8_t y=0;y<petart::CROWN_H;++y)
    stamp(out,petart::CROWN[y],petart::CROWN_W,petart::CROWN_X+dx,petart::CROWN_Y+dy+y);
}
// Upside down on the grapple cable: Cheer (arms up) flipped vertically, optional sideways swing.
void composeHang(PetCanvas& out,uint8_t e,bool crown,bool swing) {
  composeStored(out,static_cast<uint8_t>(PetPose::Cheer),e,crown);
  char tmp[PET_SPRITE_SIZE];
  for (uint8_t y=0;y<PET_SPRITE_SIZE/2;++y) {  // In-place swap: no second canvas.
    memcpy(tmp,out.rows[y],PET_SPRITE_SIZE);
    memcpy(out.rows[y],out.rows[PET_SPRITE_SIZE-1-y],PET_SPRITE_SIZE);
    memcpy(out.rows[PET_SPRITE_SIZE-1-y],tmp,PET_SPRITE_SIZE);
  }
  if (swing) for (uint8_t y=0;y<PET_SPRITE_SIZE;++y) {
    memmove(out.rows[y]+PET_HANG_SWING,out.rows[y],PET_SPRITE_SIZE-PET_HANG_SWING);
    memset(out.rows[y],'.',PET_HANG_SWING);
  }
  // The cable between the boots runs up to the top edge (the screen continues it above).
  const uint8_t x=static_cast<uint8_t>(PET_SPRITE_SIZE/2-1+(swing?PET_HANG_SWING:0));
  for (uint8_t y=0;y<PET_SPRITE_SIZE && out.rows[y][x]=='.';++y) out.rows[y][x]='w';
}
bool validRows(const char* rows,uint16_t count,uint8_t width) {
  for (uint16_t y=0;y<count;++y) {
    const char* row=rows+y*(width+1);
    if (strlen(row)!=width) return false;
    for (uint8_t x=0;x<width;++x) {
      bool known=row[x]=='.';
      for (uint8_t i=0;i<petart::INK_COUNT && !known;++i) known=row[x]==petart::INKS[i].key;
      if (!known) return false;
    }
  }
  return true;
}
}  // namespace

const SpriteInk* petInks() { return petart::INKS; }
uint8_t petInkCount() { return petart::INK_COUNT; }
uint8_t petHeadBottom() { return petart::HEAD_BOTTOM; }
uint8_t petCableRow() { return petart::CABLE_HAND_Y; }

uint8_t petHeadTop(PetPose pose) {
  const uint8_t p=static_cast<uint8_t>(pose)<STORED_POSES?static_cast<uint8_t>(pose):0;
  return static_cast<uint8_t>(petart::HEAD_TOP+petart::HEAD_OFFSET[p][1]);
}

void composePet(PetCanvas& out,const PetFrame& frame) {
  const uint8_t p=static_cast<uint8_t>(frame.pose)<POSE_COUNT?static_cast<uint8_t>(frame.pose):0;
  const uint8_t e=static_cast<uint8_t>(frame.eyes)<EYE_COUNT?static_cast<uint8_t>(frame.eyes):0;
  for (uint8_t y=0;y<PET_SPRITE_SIZE;++y) out.rows[y][PET_SPRITE_SIZE]=0;
  if (p>=STORED_POSES) composeHang(out,e,frame.crown,frame.pose==PetPose::HangSwing);
  else composeStored(out,p,e,frame.crown);
}

PetFrame petFrame(PetMood mood,PetStyle style,uint32_t moodElapsedMs,uint32_t animMs) {
  return petFrame(mood,PetReaction::None,style,moodElapsedMs,0,animMs);
}

PetFrame petFrame(PetMood mood,PetReaction reaction,PetStyle style,uint32_t moodElapsedMs,
                  uint32_t reactionElapsedMs,uint32_t animMs) {
  PetFrame f={PetPose::Relaxed,PetEyes::Open,style==PetStyle::Crown || style==PetStyle::Aura};
  const PetEyes styled=style==PetStyle::StarEyes?PetEyes::Star:style==PetStyle::HeartEyes?PetEyes::Heart:PetEyes::Open;
  const PetEyes joyful=styled==PetEyes::Open?PetEyes::Happy:styled;
  switch (reaction) {
    case PetReaction::Tickle:
      f.pose=(reactionElapsedMs/150)%2?PetPose::LaughShake:PetPose::Laugh;
      f.eyes=joyful; return f;
    case PetReaction::GrappleLeft: f.pose=PetPose::GrappleLeft; f.eyes=styled; return f;
    case PetReaction::GrappleRight: f.pose=PetPose::GrappleRight; f.eyes=styled; return f;
    case PetReaction::Hang:
      f.pose=(reactionElapsedMs/400)%2?PetPose::HangSwing:PetPose::Hang;
      f.eyes=styled; return f;  // Arched "happy" eyes would read as a frown upside down.
    default: break;
  }
  switch (mood) {
    case PetMood::Sleep:
      f.pose=(animMs/900)%2?PetPose::SleepBreathe:PetPose::Sleep;
      f.eyes=PetEyes::Sleepy; break;
    case PetMood::Happy:
      f.pose=(animMs/250)%2?PetPose::WaveFlick:PetPose::Wave;
      f.eyes=joyful; break;
    case PetMood::Celebrate:
      f.pose=(animMs/250)%2?PetPose::CheerBounce:PetPose::Cheer;
      f.eyes=joyful; break;
    default:
      f.pose=(animMs/600)%2?PetPose::Breathe:PetPose::Relaxed;
      f.eyes=(moodElapsedMs%3200)>=3050?PetEyes::Closed:styled; break;
  }
  return f;
}

uint8_t petFrameKey(const PetFrame& frame) {
  return static_cast<uint8_t>(static_cast<uint8_t>(frame.pose)*16+static_cast<uint8_t>(frame.eyes)*2+
                              (frame.crown?1:0));
}

bool petSpritesWellFormed() {
  return validRows(&petart::FRAMES[0][0][0],STORED_POSES*PET_SPRITE_SIZE,PET_SPRITE_SIZE) &&
         validRows(&petart::LENSES[0][0][0],EYE_COUNT*petart::EYE_H,petart::EYE_W) &&
         validRows(&petart::CROWN[0][0],petart::CROWN_H,petart::CROWN_W);
}
}  // namespace deskos
