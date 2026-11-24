/**
 * @file WhiteFleetClearBall.cpp
 *
 * This file defines an implementation of the WhiteFleetClearBall skill.
 *
 * @author Nico Holsten
 */

#include "WhiteFleetSkillBehaviorControl.h"
#include "Platform/BHAssert.h"
#include "Representations/Configuration/KickInfo.h"

option((WhiteFleetSkillBehaviorControl) ClearBall,
       vars((Angle)(0_deg) kickAngleInField,
            (KickInfo::KickType)(KickInfo::rlKickDynamic) kickType,
            (float)(std::numeric_limits<float>::max()) kickLength))
{
  const bool liveRestart = theGameState.isForOwnTeam() &&
    (theGameState.isGoalKick() || theGameState.isCornerKick() || theGameState.isKickIn());
  if(liveRestart)
    theClearTarget.update();

  // Should only be necessary when replaying logs
  if(!theClearTarget.isValid)
  {
    ANNOTATION("WhiteFleetClearBall", "ClearTarget was invalid!");
    OUTPUT_ERROR("ClearTarget was invalid!");
    theClearTarget.update();
  }

  initial_state(initial)
  {
    transition
    {
      if(theClearTarget.isValid && theGameState.isFreeKick())
        goto kick;
    }
    action
    {
      ASSERT(theClearTarget.isValid);
      GoToBallAndKick({.targetDirection = theClearTarget.kickAngle,
                       .kickType = theClearTarget.kickType,
                       .length = theClearTarget.kickLength});
    }
  }

  state(kick)
  {
    action
    {
      if(state_time == 0 || liveRestart)
      {
        kickAngleInField = Angle::normalize(theClearTarget.kickAngle + theRobotPose.rotation);
        kickType = !liveRestart && theClearTarget.kickType == KickInfo::rlKickDynamic
                   && theClearTarget.kickLength < kickLength
                   ? KickInfo::rlKickDynamicOld : theClearTarget.kickType;
        kickLength = theClearTarget.kickLength;
      }
      GoToBallAndKick({.targetDirection = kickAngleInField - theRobotPose.rotation,
                       .kickType = kickType,
                       .length = kickLength});
    }
  }
}
