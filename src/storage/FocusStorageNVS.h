#pragma once
#include "../services/FocusStorage.h"

namespace deskos {
class FocusStorageNVS : public IFocusStorage {
 public:
  FocusLoadResult load(FocusRecord& record) override;
  bool save(const FocusRecord& record) override;
};
}  // namespace deskos
