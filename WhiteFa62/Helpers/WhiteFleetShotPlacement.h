#pragma once
#include "Math/Angle.h"
#include "Math/Eigen.h"
#include "Math/Range.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace WhiteFleetShotPlacement
{
  struct Blocker { Vector2f position; float radius; };
  struct Params
  {
    float clearanceCap = 2000.f, keeperWeight = 1.f, postWeight = .10f, postReference = 300.f;
    float postSafety = 250.f, minClearance = 150.f, maxOffset = 800.f, nearSideLimit = 200.f;
    Angle step = 2_deg;
  };
  struct Result
  {
    Angle direction = 0_deg;
    float clearance = 0.f, postClearance = 0.f, score = 0.f;
    bool placed = false;
  };
  inline float distanceToSegment(const Vector2f& point, const Vector2f& a, const Vector2f& b)
  {
    const Vector2f ab = b - a;
    const float lengthSquared = ab.squaredNorm();
    if(lengthSquared < 1.f)
      return (point - a).norm();
    const float t = std::max(0.f, std::min(1.f, (point - a).dot(ab) / lengthSquared));
    return (point - (a + ab * t)).norm();
  }
  // Share scoring while retaining both the platform's three points and the legacy sampler.
  inline Result selectDirection(const Rangea& range, const Vector2f& ball, const std::vector<Blocker>& blockers,
                                float goalX, float halfGoal, const Params& p, bool three, const Angle* previous)
  {
    Result result, fallback;
    if(!three) result.direction = range.getCenter();
    const float depth = goalX - ball.x();
    if(halfGoal <= p.postSafety || (three ? !ball.allFinite() || depth <= 0.f : depth < 100.f))
      return result;
    float best = -std::numeric_limits<float>::max(), fallbackClearance = 0.f;
    const auto consider = [&](Angle direction, float y)
    {
      if(three && !range.isInside(direction)) return;
      const float post = halfGoal - std::abs(y) - p.postSafety;
      if(three ? post <= 0.f : post < 0.f || std::abs(y) > p.maxOffset) return;
      if(!three && std::abs(ball.y()) > halfGoal && y * (ball.y() > 0.f ? 1.f : -1.f) > p.nearSideLimit) return;
      float clearance = std::numeric_limits<float>::max();
      for(const Blocker& blocker : blockers)
        if(!three || (blocker.position.allFinite() && std::isfinite(blocker.radius)))
          clearance = std::min(clearance, distanceToSegment(blocker.position, ball, {goalX, y}) - blocker.radius);
      if(three ? clearance < p.minClearance : clearance <= 0.f) return;
      const float postPenalty = p.postWeight * p.postReference / (three ? post : std::max(1.f, post));
      float score = three ? std::min(clearance, p.clearanceCap) / p.clearanceCap - postPenalty :
        -(p.keeperWeight * std::max(0.f, 1.f - clearance / p.clearanceCap) + postPenalty);
      if(three && previous && std::abs(Angle::normalize(direction - *previous)) < .5_deg) score += .08f;
      if(clearance >= p.minClearance && score > best)
      {
        result = {direction, clearance, post, score, true};
        best = score;
      }
      if(!three && clearance > fallbackClearance)
      {
        fallback = {direction, clearance, post, score, true};
        fallbackClearance = clearance;
      }
    };
    if(three)
      for(const float y : {0.f, halfGoal * .5f, -halfGoal * .5f})
        consider(Angle((Vector2f(goalX, y) - ball).angle()), y);
    else
    {
      const float step = static_cast<float>(p.step), size = static_cast<float>(range.getSize());
      const int steps = std::max(1, static_cast<int>(std::round(size / step)));
      for(int i = 0; i <= steps; ++i)
      {
        const Angle angle(static_cast<float>(range.min) + std::min(step * static_cast<float>(i), size));
        if(std::abs(static_cast<float>(angle)) <= static_cast<float>(pi_2))
          consider(angle, ball.y() + std::tan(static_cast<float>(angle)) * depth);
      }
    }
    return three || result.placed ? result : fallback;
  }
  inline Result bestThreeDirection(const Rangea& range, const Vector2f& ball, const std::vector<Blocker>& blockers,
                                   float goalX, float halfGoal, const Params& p = Params(), const Angle* previous = nullptr)
  { return selectDirection(range, ball, blockers, goalX, halfGoal, p, true, previous); }
  inline Result bestDirection(const Rangea& range, const Vector2f& ball, const std::vector<Blocker>& blockers,
                              float goalX, float halfGoal, const Params& p = Params())
  { return selectDirection(range, ball, blockers, goalX, halfGoal, p, false, nullptr); }
}
