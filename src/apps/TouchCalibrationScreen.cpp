#include "TouchCalibrationScreen.h"
#include <cstdio>

void TouchCalibrationScreen::begin(uint32_t now) {
  stage_=Stage::Collect; point_=0; pointAt_=lastProgress_=now;
  waitForRelease_=true; sawPress_=false; resultTaken_=false; samples_.reset();
  Serial.println("TOUCH_CAL: START");
  Serial.println("TOUCH_CAL: POINT 1/5");
  drawTarget();
}

void TouchCalibrationScreen::drawTarget() {
  auto& tft=display_.graphics();
  tft.fillScreen(TFT_BLACK); tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM); tft.setTextColor(TFT_WHITE,TFT_BLACK);
  char text[64];
  const bool verify=stage_==Stage::Verify;
  snprintf(text,sizeof(text),"%s %u/%u",verify?"VERIFY":"TOUCH CAL",point_+1,
           verify?3:5);
  tft.drawString(text,8,8);
  const TouchPixel target=verify?touchcal::VERIFY_TARGETS[point_]:touchcal::TARGETS[point_];
  tft.drawString("Touch +, hold ~0.5s, then release.",8,72);
  tft.drawString("Release between targets. Timeout: 30s",8,84);
  // Verification targets are at y=60/80/160; keep text away from their crosses.
  if (verify) {
    tft.fillRect(8,70,304,24,TFT_BLACK);
    tft.drawString("Touch +, hold, then release.",8,100);
  }
  tft.drawString("Serial: CAL / RETRY / DIAG / TEST",8,228);
  // Draw the single marker last, including verification targets near text bands.
  tft.drawFastHLine(target.x-10,target.y,21,TFT_YELLOW);
  tft.drawFastVLine(target.x,target.y-10,21,TFT_YELLOW);
}

void TouchCalibrationScreen::showFailure(const char* message) {
  stage_=Stage::Failed;
  auto& tft=display_.graphics();
  tft.fillScreen(TFT_BLACK); tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM); tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("TOUCH CAL - NOT SAVED",10,20);
  tft.drawString(message,10,60);
  tft.drawString("Release, then tap anywhere to retry.",10,90);
  tft.drawString("Serial RETRY or CAL also works.",10,108);
  tft.drawRect(100,170,120,40,TFT_WHITE);
  tft.drawString("RETRY",144,186);
  tft.drawString("DIAG: display recovery | TEST: old map",10,226);
  waitForRelease_=true;
}

void TouchCalibrationScreen::storageFailed() {
  Serial.println("TOUCH_CAL: NVS_SAVE_FAIL");
  showFailure("NVS write/readback failed. Retry CAL.");
}

void TouchCalibrationScreen::accept(const RawTouchPoint& median, uint32_t now) {
  if (stage_==Stage::Collect) measured_[point_]=median;
  else {
    verification_[point_]=median;
    const float error=touchcal::errorAt(candidate_,median,touchcal::VERIFY_TARGETS[point_]);
    Serial.printf("TOUCH_CAL: VERIFY_ERROR point=%u error_px=%.2f\n",point_+1,error);
    if (error>15) {
      Serial.println("TOUCH_CAL: VALIDATION_FAIL independent_error");
      showFailure("Independent error >15px. CAL again.");
      return;
    }
  }
  Serial.println("TOUCH_CAL: POINT_ACCEPTED");
  ++point_;
  if (stage_==Stage::Collect && point_==touchcal::POINT_COUNT) {
    if (!touchcal::fit(measured_,candidate_)) {
      Serial.println("TOUCH_CAL: VALIDATION_FAIL fit_or_residual");
      showFailure("Bad fit / noisy or misplaced targets.");
      return;
    }
    Serial.printf("TOUCH_CAL: FIT rms_px=%.2f max_px=%.2f\n",candidate_.rmsError,candidate_.maxError);
    stage_=Stage::Verify; point_=0;
  } else if (stage_==Stage::Verify && point_==touchcal::VERIFY_COUNT) {
    if (!touchcal::verify(candidate_,verification_)) {
      Serial.println("TOUCH_CAL: VALIDATION_FAIL independent_check");
      showFailure("Independent validation failed.");
      return;
    }
    Serial.println("TOUCH_CAL: VALIDATION_PASS");
    stage_=Stage::Complete;
    return;
  }
  pointAt_=now; samples_.reset();
  Serial.printf("TOUCH_CAL: %s %u/%u\n",stage_==Stage::Verify?"VERIFY":"POINT",
                point_+1,stage_==Stage::Verify?3:5);
  drawTarget();
}

void TouchCalibrationScreen::update(const TouchFrame& frame, uint32_t now) {
  if (stage_==Stage::Complete) return;
  if (stage_==Stage::Failed) {
    if (!frame.updated) return;
    if (waitForRelease_) {
      if (!frame.down && !frame.rawValid) waitForRelease_=false;
    } else if (frame.events & TOUCH_TAP) begin(now);
    return;
  }
  if (now-pointAt_>=30000) {
    Serial.println("TOUCH_CAL: TIMEOUT"); showFailure("No valid point within 30 seconds."); return;
  }
  if (!frame.updated) return;
  if (waitForRelease_) {
    if (!frame.down && !frame.rawValid) waitForRelease_=false;
    return;
  }
  if (frame.events & TOUCH_PRESS) { samples_.reset(); sawPress_=true; }
  if (sawPress_ && frame.down && frame.rawValid && frame.heldMs>=80) samples_.add(frame.raw);
  if (frame.events & TOUCH_RELEASE) {
    if (!sawPress_) return;
    sawPress_=false;
    RawTouchPoint median; uint8_t inliers=0;
    if (samples_.median(median,inliers)) {
      Serial.printf("TOUCH_CAL: RAW x=%d y=%d z=%d samples=%u inliers=%u\n",
                    median.x,median.y,median.z,samples_.count(),inliers);
      accept(median,now);
    } else {
      Serial.printf("TOUCH_CAL: SAMPLE_REJECTED samples=%u inliers=%u\n",samples_.count(),inliers);
      samples_.reset(); drawTarget();
    }
  }
  if (stage_!=Stage::Failed && stage_!=Stage::Complete && now-lastProgress_>=200) {
    lastProgress_=now;
    auto& tft=display_.graphics();
    // Above the bottom targets; avoid independent verification target at y=160.
    const int y=stage_==Stage::Verify?190:156;
    tft.fillRect(8,y,304,12,TFT_BLACK);
    char line[64]; snprintf(line,sizeof(line),"Samples %u/31 | Hold steady, release",samples_.count());
    tft.drawString(line,8,y);
  }
}

bool TouchCalibrationScreen::takeResult(touchcal::CalibrationRecord& result) {
  if (stage_!=Stage::Complete || resultTaken_) return false;
  resultTaken_=true; result=candidate_; return true;
}
