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
#include "WhiteFleetKickPower.h"

option((WhiteFleetSkillBehaviorControl) ClearBall,
       vars((Angle)(0_deg) kickAngleInField,
            (KickInfo::KickType)(KickInfo::rlKickDynamic) kickType,
            (float)(std::numeric_limits<float>::max()) kickLength))
{
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
                       .kickType = SystemCall::getMode() == SystemCall::simulatedRobot ? KickInfo::rlKickStrong : theClearTarget.kickType,
                       .length = SystemCall::getMode() == SystemCall::simulatedRobot ? WhiteFleetKickPower::fullLength() : theClearTarget.kickLength});
    }
  }

  state(kick)
  {
    action
    {
      if(state_time == 0)
      {
        kickAngleInField = Angle::normalize(theClearTarget.kickAngle + theRobotPose.rotation);
        kickType = theClearTarget.kickType == KickInfo::rlKickDynamic
                   && theClearTarget.kickLength < kickLength
                   ? KickInfo::rlKickDynamicOld : theClearTarget.kickType;
        kickLength = theClearTarget.kickLength;
      }
      GoToBallAndKick({.targetDirection = kickAngleInField - theRobotPose.rotation,
                       .kickType = SystemCall::getMode() == SystemCall::simulatedRobot ? KickInfo::rlKickStrong : kickType,
                       .length = SystemCall::getMode() == SystemCall::simulatedRobot ? WhiteFleetKickPower::fullLength() : kickLength});
    }
  }
}
