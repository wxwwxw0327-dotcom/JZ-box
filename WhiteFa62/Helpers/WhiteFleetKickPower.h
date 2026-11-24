#pragma once

#include "Math/Eigen.h"
#include "Representations/Configuration/BallSpecification.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace WhiteFleetKickPower
{
  inline float fullLength() { return std::numeric_limits<float>::max(); }

  inline float passLength(float distance, const BallSpecification& ball)
  {
    if(!std::isfinite(distance) || distance <= 0.f)
      return 500.f;
    distance = std::clamp(distance, 200.f, 20000.f);
    const float arrivalSpeed = std::clamp(900.f + distance * .05f, 950.f, 1400.f);
    const float deceleration = -ball.staticFriction * 1000.f;
    const float extra = std::isfinite(deceleration) && deceleration > 1.f ?
      arrivalSpeed * arrivalSpeed / (2.f * deceleration) : 500.f;
    return std::min(40000.f, distance + extra);
  }

}
