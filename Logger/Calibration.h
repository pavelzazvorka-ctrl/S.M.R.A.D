#pragma once

#include <Arduino.h>
#include <time.h>

#include "ConfigManager.h"
#include "DebugLog.h"

float calibrate(float input, const CalibrationConfig* cal);
