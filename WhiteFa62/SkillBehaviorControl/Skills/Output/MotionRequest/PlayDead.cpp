/**
 * @file WhiteFleetPlayDead.cpp
 *
 * This file implements the WhiteFleetPlayDead skill.
 *
 * @author Arne Hasselbring
 */

#include "WhiteFleetSkillBehaviorControl.h"

option((WhiteFleetSkillBehaviorControl) PlayDead)
{
  theMotionRequest.motion = MotionRequest::playDead;
  theLibCheck.inc(LibCheck::motionRequest);

  initial_state(execute)
  {
    transition
    {
      if(theMotionInfo.executedPhase == MotionPhase::playDead)
        goto done;
    }
  }

  target_state(done) {}
}
