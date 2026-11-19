/**
 * @file ClearBall.cpp
 *
 * This file defines an implementation of the ClearBall skill.
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
  // Should only be necessary when replaying logs
  if(!theClearTarget.isValid)
  {
    ANNOTATION("ClearBall", "ClearTarget was invalid!");
    OUTPUT_ERROR("ClearTarget was invalid!");
    theClearTarget.update();
  }

  initial_state(initial)
  {
    transition
    {
      // MDFZ v39: 只有自己罚任意球才走 kick 分支, 对方任意球不该我去踢
      if(theClearTarget.isValid && theGameState.isFreeKick() && theGameState.isForOwnTeam())
        goto kick;
    }
    action
    {
      // MDFZ v39: 原来是 ASSERT(theClearTarget.isValid). 平台断言开着时, 目标失效
      // 一帧就会 abort 打死机器人进程. 改成"看球站住", 等目标恢复再踢.
      if(theClearTarget.isValid)
        GoToBallAndKick({.targetDirection = theClearTarget.kickAngle,
                         .kickType = theClearTarget.kickType,
                         .length = theClearTarget.kickLength});
      else
      {
        LookActive({.withBall=true});
        Stand();
      }
    }
  }

  state(kick)
  {
    transition
    {
      // MDFZ v39: 原来 kick 状态没有任何转移, 进来以后整场出不去, 每帧追着球踢.
      if(!theGameState.isFreeKick() || state_time > 4000)
        goto initial;
    }
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
                       .kickType = kickType,
                       .length = kickLength});
    }
  }
}
