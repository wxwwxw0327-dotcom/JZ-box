/**
 * @file ShotPlacement.h
 *
 * Selects a shot direction inside the goal that keeps away from blockers (usually the
 * goalkeeper) instead of aiming at the goal centre.
 *
 * The native target selection clamps the direction to the most central point of the free
 * sector, i.e. towards the goal centre. When a goalkeeper stands in the centre, that means
 * shooting straight at it. This heuristic samples directions inside the allowed sector and
 * scores them with two penalties:
 *
 *  - the keeper penalty is linear in the clearance (0 at clearanceCap, maximal at zero
 *    clearance), so within clearanceCap of a blocker the score drops proportionally;
 *  - the post penalty is asymptotic: it is proportional to 1 / distance to the post, i.e.
 *    it has a pole at the post, so every additional millimetre closer to the post costs more.
 *
 * Without blockers the keeper penalty is zero and the post penalty alone is minimal in the
 * centre of the goal, so the native behaviour is kept.
 */

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
  struct Blocker
  {
    Vector2f position; /**< Blocker position on the field. */
    float radius; /**< Half of the blocker width. */
  };

  struct Params
  {
    float clearanceCap = 2000.f; /**< Beyond this clearance the keeper penalty is zero (mm). */
    float keeperWeight = 1.f; /**< Weight of the linear keeper penalty. */
    float postWeight = 0.10f; /**< Weight of the asymptotic post penalty. */
    float postReference = 300.f; /**< Post penalty at a post clearance of this value (mm). */
    float postSafety = 250.f; /**< Minimal clearance between shot line and goal post (mm). */
    float minClearance = 150.f; /**< Preferred minimal clearance to a blocker (mm). */
    /** Round 2 (MDFZ runs #85/#86/#88): aims > 0.9 m off the goal centre missed 41-63 % (sim kick spread +-6 deg),
     *  while 0.3-0.9 m scored best (36-39 %, 8-11 % misses, run #84). So the aim stays within maxOffset of the centre. */
    float maxOffset = 800.f;
    /** Round 2: from outside the posts' lane (|ball y| > half goal width) the near post is a small target the keeper
     *  reaches in one step and the kick spread sends wide (run #85 pictures): no aim more than this towards the near post. */
    float nearSideLimit = 200.f;
    Angle step = 2_deg; /**< Sampling resolution. */
  };

  struct Result
  {
    Angle direction = 0_deg; /**< Field direction of the chosen shot. */
    float clearance = 0.f; /**< Clearance of the chosen direction to the nearest blocker. */
    float postClearance = 0.f; /**< Distance of the impact point to the nearest goal post. */
    float score = 0.f; /**< Score of the chosen direction (negative penalty sum). */
    bool placed = false; /**< False if no valid direction was found. */
  };

  /**
   * Distance between a point and a line segment.
   */
  inline float distanceToSegment(const Vector2f& point, const Vector2f& a, const Vector2f& b)
  {
    const Vector2f ab = b - a;
    const float lengthSquared = ab.squaredNorm();
    if(lengthSquared < 1.f)
      return (point - a).norm();
    const float t = std::max(0.f, std::min(1.f, (point - a).dot(ab) / lengthSquared));
    return (point - (a + ab * t)).norm();
  }

  /**
   * Chooses the field direction of a shot inside the given search range.
   * @param searchRange The allowed field directions.
   * @param ballOnField The ball position on the field.
   * @param blockers Obstacles that must not be hit (field coordinates).
   * @param goalLineX The x coordinate of the goal line.
   * @param halfGoalWidth Half of the goal width.
   * @return The chosen direction. placed is false if no candidate is physically feasible.
   */
  inline Result bestDirection(const Rangea& searchRange, const Vector2f& ballOnField,
                              const std::vector<Blocker>& blockers, const float goalLineX,
                              const float halfGoalWidth, const Params& params = Params())
  {
    Result result;
    result.direction = searchRange.getCenter();

    const float depth = goalLineX - ballOnField.x();
    if(depth < 100.f || halfGoalWidth <= params.postSafety)
      return result;

    const float step = static_cast<float>(params.step);
    const float rangeSize = static_cast<float>(searchRange.getSize());
    const int steps = std::max(1, static_cast<int>(std::ceil(rangeSize / step)));
    float bestScore = -std::numeric_limits<float>::max();
    Result fallback;
    float fallbackClearance = 0.f;

    // Include the exact goal-centre ray so an empty goal has no sampling bias.
    const Angle centerDirection = searchRange.limit(Angle(std::atan2(-ballOnField.y(), depth)));
    for(int i = 0; i <= steps + 1; ++i)
    {
      const Angle candidate = i == steps + 1 ? centerDirection :
        Angle(static_cast<float>(searchRange.min) + std::min(step * static_cast<float>(i), rangeSize));
      const float angle = static_cast<float>(candidate);
      if(std::abs(angle) > static_cast<float>(pi_2))
        continue;

      const float y = ballOnField.y() + std::tan(angle) * depth;
      const float postClearance = halfGoalWidth - std::abs(y) - params.postSafety;
      if(postClearance < 0.f || std::abs(y) > params.maxOffset)
        continue;
      if(std::abs(ballOnField.y()) > halfGoalWidth && y * (ballOnField.y() > 0.f ? 1.f : -1.f) > params.nearSideLimit)
        continue;

      const Vector2f target(goalLineX, y);
      float clearance = std::numeric_limits<float>::max();
      for(const Blocker& blocker : blockers)
        clearance = std::min(clearance, distanceToSegment(blocker.position, ballOnField, target) - blocker.radius);
      if(clearance <= 0.f)
        continue;

      // Linear keeper penalty: fades out linearly with the clearance.
      const float keeperPenalty = params.keeperWeight * std::max(0.f, 1.f - clearance / params.clearanceCap);
      // Asymptotic post penalty: proportional to 1 / distance, so it has a pole at the post.
      const float postPenalty = params.postWeight * params.postReference / std::max(1.f, postClearance);
      const float score = -(keeperPenalty + postPenalty);

      if(clearance >= params.minClearance && score > bestScore)
      {
        bestScore = score;
        result.direction = candidate;
        result.clearance = clearance;
        result.postClearance = postClearance;
        result.score = score;
        result.placed = true;
      }
      // If every direction is tight, at least take the one with the most clearance.
      if(clearance > fallbackClearance)
      {
        fallbackClearance = clearance;
        fallback.direction = candidate;
        fallback.clearance = clearance;
        fallback.postClearance = postClearance;
        fallback.score = score;
        fallback.placed = true;
      }
    }

    return result.placed ? result : fallback;
  }
}
