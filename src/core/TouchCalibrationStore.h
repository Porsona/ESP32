#pragma once
#include "TouchCalibration.h"

namespace touchcal {
bool load(CalibrationRecord& record);
bool save(const CalibrationRecord& record);
}  // namespace touchcal
