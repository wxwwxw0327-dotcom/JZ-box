/**
 * @file WhiteFleetCalibrationFinished.cpp
 *
 * This file implements the WhiteFleetCalibrationFinished skill.
 *
 * @author Arne Hasselbring
 */

#include "WhiteFleetSkillBehaviorControl.h"

option((WhiteFleetSkillBehaviorControl) CalibrationFinished)
{
  initial_state(execute)
  {
    action
    {
      theBehaviorStatus.calibrationFinished = true;
      theLibCheck.inc(LibCheck::calibrationFinished);
    }
  }
}
