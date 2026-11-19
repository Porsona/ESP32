#pragma once
// Shared host-test helpers: fake clock, fake blob store, XP injection and a
// global allocation counter (include this header in exactly one .cpp per exe).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "services/BlobStore.h"
#include "services/FocusStorage.h"
#include "services/ProgressService.h"

namespace deskos {
struct ProgressTestAccess {
  static void setFocusXP(ProgressService& p,uint32_t xp) { ProgressData d=p.data(); d.totalXP=xp; p.adopt(d); }
  static void setBonusXP(ProgressService& p,uint32_t xp) {
    RewardLedger l=p.ledger(); l.bonusXP=xp; l.grantedCount=(xp+99)/100; if (!l.openDayId) l.openDayId=1; p.adoptLedger(l);
  }
};
}  // namespace deskos

struct TestClock : deskos::IFocusClock {
  uint32_t time=1000;
  uint32_t nowMs() const override { return time; }
};

struct FakeBlobStore : deskos::IBlobStore {
  struct Slot { char ns[16]; char key[16]; size_t size; unsigned char data[512]; bool used; };
  Slot slots[12];
  bool failWrites=false,readError=false;
  unsigned saves=0,writes=0;
  FakeBlobStore() { std::memset(slots,0,sizeof(slots)); }
  Slot* find(const char* ns,const char* key) {
    for (auto& s:slots) if (s.used && !std::strcmp(s.ns,ns) && !std::strcmp(s.key,key)) return &s;
    return nullptr;
  }
  deskos::BlobLoad load(const char* ns,const char* key,void* out,size_t size) override {
    if (readError) return deskos::BlobLoad::IoError;
    Slot* s=find(ns,key);
    if (!s) return deskos::BlobLoad::Missing;
    if (s->size!=size) return deskos::BlobLoad::Invalid;
    std::memcpy(out,s->data,size); return deskos::BlobLoad::Loaded;
  }
  bool save(const char* ns,const char* key,const void* data,size_t size) override {
    ++saves;
    if (failWrites || size>512) return false;
    Slot* s=find(ns,key);
    if (!s) for (auto& f:slots) if (!f.used) { s=&f; break; }
    if (!s) return false;
    std::snprintf(s->ns,sizeof(s->ns),"%s",ns); std::snprintf(s->key,sizeof(s->key),"%s",key);
    s->size=size; std::memcpy(s->data,data,size); s->used=true; ++writes;
    return true;
  }
  void corrupt(const char* ns,const char* key,size_t offset) { Slot* s=find(ns,key); if (s && offset<s->size) s->data[offset]^=0x5A; }
};

// Counts heap allocations so steady-state loops can assert "no allocation".
extern unsigned long g_allocations;
#ifdef DESKOS_TEST_ALLOC_COUNTER
unsigned long g_allocations=0;
void* operator new(std::size_t n) { ++g_allocations; void* p=std::malloc(n?n:1); if (!p) throw std::bad_alloc(); return p; }
void* operator new[](std::size_t n) { ++g_allocations; void* p=std::malloc(n?n:1); if (!p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
#endif

#define CHECK(expression) do { if (!(expression)) { std::printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expression); ++failures; return; } } while (0)
