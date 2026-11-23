/**
 * @file WhiteFleetPassTarget.cpp
 *
 * This file implements the WhiteFleetPassTarget skill.
 *
 * @author Arne Hasselbring
 */

#include "WhiteFleetSkillBehaviorControl.h"

option((WhiteFleetSkillBehaviorControl) PassTarget,
       args((int) passTarget,
            (const Vector2f&) ballTarget))
{
  initial_state(execute)
  {
    action
    {
      theBehaviorStatus.passTarget = passTarget;
      theBehaviorStatus.shootingTo = ballTarget;
      theLibCheck.inc(LibCheck::passTarget);
    }
  }
}
