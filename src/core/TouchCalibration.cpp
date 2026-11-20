#include "TouchCalibration.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace touchcal {
const TouchPixel TARGETS[POINT_COUNT] = {{30,42}, {290,42}, {160,120}, {30,208}, {290,208}};
const TouchPixel VERIFY_TARGETS[VERIFY_COUNT] = {{80,80}, {240,160}, {160,60}};
namespace {
constexpr uint32_t MAGIC = 0x5443414CUL;
bool coordinateValid(int x, int y) { return x > 0 && x < 4095 && y > 0 && y < 4095; }
bool geometryValid(const CalibrationRecord& r) {
  int xmin = 4095, xmax = 0, ymin = 4095, ymax = 0;
  for (size_t i = 0; i < POINT_COUNT; ++i) {
    const int x = r.raw[i][0], y = r.raw[i][1];
    if (!coordinateValid(x,y)) return false;
    xmin = std::min(xmin,x); xmax = std::max(xmax,x);
    ymin = std::min(ymin,y); ymax = std::max(ymax,y);
  }
  // Reject clustered taps. These are minimum spans, not assumed mapping endpoints.
  return xmax - xmin >= 200 && ymax - ymin >= 200;
}
}  // namespace

uint32_t checksum(const CalibrationRecord& r) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&r);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < offsetof(CalibrationRecord, checksum); ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
  }
  return ~crc;
}

bool project(const CalibrationRecord& r, const RawTouchPoint& raw, float& x, float& y) {
  const auto* c = r.coefficient;
  x = c[0]*raw.x + c[1]*raw.y + c[2];
  y = c[3]*raw.x + c[4]*raw.y + c[5];
  return std::isfinite(x) && std::isfinite(y);
}

float errorAt(const CalibrationRecord& r, const RawTouchPoint& raw, const TouchPixel& target) {
  float x, y;
  if (!project(r,raw,x,y)) return INFINITY;
  return std::hypot(x-target.x, y-target.y);  // Never clamp before residual checking.
}

TouchPixel map(const CalibrationRecord& r, const RawTouchPoint& raw) {
  float x, y;
  TouchPixel pixel;
  if (project(r,raw,x,y)) {
    pixel.x = static_cast<int16_t>(std::lround(std::max(0.0f,std::min(319.0f,x))));
    pixel.y = static_cast<int16_t>(std::lround(std::max(0.0f,std::min(239.0f,y))));
  }
  return pixel;
}

bool fit(const RawTouchPoint points[POINT_COUNT], CalibrationRecord& r) {
  std::memset(&r,0,sizeof(r));
  r.magic = MAGIC; r.version = 1; r.bytes = sizeof(r);
  r.width = 320; r.height = 240; r.rotation = 1;
  double mx=0, my=0, tx=0, ty=0;
  for (size_t i=0; i<POINT_COUNT; ++i) {
    if (!validRawTouch(points[i])) return false;
    r.raw[i][0]=points[i].x; r.raw[i][1]=points[i].y;
    mx += points[i].x/4095.0; my += points[i].y/4095.0;
    tx += TARGETS[i].x; ty += TARGETS[i].y;
  }
  if (!geometryValid(r)) return false;
  mx/=POINT_COUNT; my/=POINT_COUNT; tx/=POINT_COUNT; ty/=POINT_COUNT;
  double xx=0, xy=0, yy=0, ux=0, vx=0, uy=0, vy=0;
  for (size_t i=0; i<POINT_COUNT; ++i) {
    const double x=points[i].x/4095.0-mx, y=points[i].y/4095.0-my;
    xx+=x*x; xy+=x*y; yy+=y*y;
    ux+=x*(TARGETS[i].x-tx); vx+=y*(TARGETS[i].x-tx);
    uy+=x*(TARGETS[i].y-ty); vy+=y*(TARGETS[i].y-ty);
  }
  const double det=xx*yy-xy*xy;
  if (!std::isfinite(det) || det < 1e-6) return false;
  const double a=(ux*yy-vx*xy)/det, b=(vx*xx-ux*xy)/det;
  const double d=(uy*yy-vy*xy)/det, e=(vy*xx-uy*xy)/det;
  r.coefficient[0]=a/4095.0; r.coefficient[1]=b/4095.0; r.coefficient[2]=tx-a*mx-b*my;
  r.coefficient[3]=d/4095.0; r.coefficient[4]=e/4095.0; r.coefficient[5]=ty-d*mx-e*my;
  float sum=0;
  for (size_t i=0; i<POINT_COUNT; ++i) {
    const float error=errorAt(r,points[i],TARGETS[i]);
    sum+=error*error; r.maxError=std::max(r.maxError,error);
  }
  r.rmsError=std::sqrt(sum/POINT_COUNT);
  r.checksum=checksum(r);
  return validRecord(r,false);
}

bool validRecord(const CalibrationRecord& r, bool requireVerification) {
  if (r.magic!=MAGIC || r.version!=1 || r.bytes!=sizeof(r) || r.width!=320 || r.height!=240 ||
      r.rotation!=1 || r.reserved[0] || r.reserved[1] || r.checksum!=checksum(r)) return false;
  if (!geometryValid(r)) return false;
  for (int i=0;i<6;++i) {
    if (!std::isfinite(r.coefficient[i]) || std::fabs(r.coefficient[i]) > (i%3==2 ? 10000 : 10)) return false;
  }
  // A singular affine cannot describe both display axes.
  if (std::fabs(r.coefficient[0]*r.coefficient[4]-r.coefficient[1]*r.coefficient[3]) < 1e-8f) return false;
  float sum=0, maximum=0;
  for (size_t i=0;i<POINT_COUNT;++i) {
    RawTouchPoint p; p.x=r.raw[i][0]; p.y=r.raw[i][1];
    const float error=errorAt(r,p,TARGETS[i]);
    if (!std::isfinite(error)) return false;
    sum+=error*error; maximum=std::max(maximum,error);
  }
  const float rms=std::sqrt(sum/POINT_COUNT);
  if (!std::isfinite(r.rmsError) || !std::isfinite(r.maxError) || rms>8 || maximum>12 ||
      std::fabs(r.rmsError-rms)>0.05f || std::fabs(r.maxError-maximum)>0.05f) return false;
  if (r.verificationCount==0 && !requireVerification) return true;
  if (r.verificationCount!=VERIFY_COUNT) return false;
  maximum=0;
  for (size_t i=0;i<VERIFY_COUNT;++i) {
    RawTouchPoint p; p.x=r.verificationRaw[i][0]; p.y=r.verificationRaw[i][1];
    if (!coordinateValid(p.x,p.y)) return false;
    const float error=errorAt(r,p,VERIFY_TARGETS[i]);
    if (!std::isfinite(error)) return false;
    maximum=std::max(maximum,error);
  }
  return std::isfinite(r.verificationMaxError) && maximum<=15 &&
         std::fabs(r.verificationMaxError-maximum)<=0.05f;
}

bool verify(CalibrationRecord& r, const RawTouchPoint points[VERIFY_COUNT]) {
  if (!validRecord(r,false)) return false;
  r.verificationMaxError=0;
  for (size_t i=0;i<VERIFY_COUNT;++i) {
    if (!validRawTouch(points[i])) return false;
    r.verificationRaw[i][0]=points[i].x; r.verificationRaw[i][1]=points[i].y;
    r.verificationMaxError=std::max(r.verificationMaxError,errorAt(r,points[i],VERIFY_TARGETS[i]));
  }
  r.verificationCount=VERIFY_COUNT;
  r.checksum=checksum(r);
  return validRecord(r);
}

void StableSamples::add(const RawTouchPoint& p) {
  if (!validRawTouch(p)) return;
  samples_[next_]=p; next_=(next_+1)%31;
  if (count_<31) ++count_;
}

bool StableSamples::median(RawTouchPoint& result, uint8_t& inliers) const {
  inliers=0;
  if (count_<21) return false;
  int16_t xs[31], ys[31], zs[31];
  for (int i=0;i<count_;++i) { xs[i]=samples_[i].x; ys[i]=samples_[i].y; }
  std::sort(xs,xs+count_); std::sort(ys,ys+count_);
  const int mx=xs[count_/2], my=ys[count_/2];
  for (int i=0;i<count_;++i) {
    const auto& p=samples_[i];
    if (std::abs(p.x-mx)<=90 && std::abs(p.y-my)<=90) {
      xs[inliers]=p.x; ys[inliers]=p.y; zs[inliers]=p.z; ++inliers;
    }
  }
  if (inliers<21 || inliers*5<count_*4) return false;
  std::sort(xs,xs+inliers); std::sort(ys,ys+inliers); std::sort(zs,zs+inliers);
  if (xs[inliers-1]-xs[0]>120 || ys[inliers-1]-ys[0]>120) return false;
  result.x=xs[inliers/2]; result.y=ys[inliers/2]; result.z=zs[inliers/2];
  return true;
}
}  // namespace touchcal
