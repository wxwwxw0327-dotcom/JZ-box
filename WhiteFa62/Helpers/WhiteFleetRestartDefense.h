#pragma once

#include "Math/Eigen.h"
#include <algorithm>
#include <cmath>
#include <optional>

/** Two separated, legal screens for an opponent's restart in our half. */
namespace WhiteFleetRestartDefense
{
  struct Layout
  {
    std::optional<Vector2f> pass;
    std::optional<Vector2f> shot;
  };

  template<typename Legal>
  std::optional<Vector2f> lanePoint(const Vector2f& ball, const Vector2f& end, float preferred,
                                  float ownGoalX, float opponentGoalX, float halfWidth, float keepAway,
                                  const std::optional<Vector2f>& reserved, const Legal& legal)
  {
    const Vector2f lane = end - ball;
    const float length = lane.norm();
    if(!ball.allFinite() || !end.allFinite() || length < keepAway + 600.f)
      return std::nullopt;
    preferred = std::clamp(preferred, keepAway, length - 600.f);
    for(const float offset : {0.f, -500.f, 500.f, -1000.f, 1000.f, -1500.f, 1500.f, -2000.f, 2000.f})
    {
      const float along = preferred + offset;
      if(along < keepAway || along > length - 600.f)
        continue;
      Vector2f candidate = ball + lane * (along / length);
      // A corner can lie on the goal line itself. Keep the body inside the pitch.
      candidate.x() = std::clamp(candidate.x(), ownGoalX + 250.f, opponentGoalX - 250.f);
      candidate.y() = std::clamp(candidate.y(), -halfWidth + 250.f, halfWidth - 250.f);
      if((candidate - ball).norm() < keepAway ||
         (reserved && (candidate - *reserved).norm() < 1000.f) || !legal(candidate))
        continue;
      return candidate;
    }
    return std::nullopt;
  }

  template<typename Legal>
  Layout plan(const Vector2f& ball, const Vector2f& receiver, float ownGoalX, float opponentGoalX,
              float halfWidth, float keepAway, const Legal& legal)
  {
    Layout result;
    if(!ball.allFinite() || !receiver.allFinite() || !std::isfinite(keepAway) || keepAway <= 0.f)
      return result;
    const Vector2f goal(ownGoalX, 0.f);
    result.shot = lanePoint(ball, goal, .7f * (goal - ball).norm(), ownGoalX, opponentGoalX,
                            halfWidth, keepAway, std::nullopt, legal);
    result.pass = lanePoint(ball, receiver, .5f * (receiver - ball).norm(), ownGoalX, opponentGoalX,
                            halfWidth, keepAway, result.shot, legal);
    return result;
  }
}
