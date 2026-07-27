#pragma once

#include <Arduino.h>
#include <time.h>

#include "ConfigManager.h"
#include "DebugLog.h"
#include <math.h>

float calibrate(float x, const CalibrationConfig* cal)
{
    float y = x;

    if (cal == nullptr)
        return y;

    if (cal->lowerLimit != cal->upperLimit)
    {
      if (cal->lowerLimit > x || cal->upperLimit < x)
      return 0.00;
    }
 
    switch (cal->cType)
    {
        case 1:
            // Polynomial:
            // y = g + a*x + b*x^2 + c*x^3 + d*x^4 + e*x^5 + f*x^6

            y =
                cal->g +
                cal->a * x +
                cal->b * powf(x, 2) +
                cal->c * powf(x, 3) +
                cal->d * powf(x, 4) +
                cal->e * powf(x, 5) +
                cal->f * powf(x, 6);

            break;


        case 2:
            // Exponential:
            // y = a * e^(b*x) + c*x + d

            y =
                cal->a * expf(cal->b * x) +
                cal->c * x +
                cal->d;

            break;


        case 3:
            // Power:
            // y = a * (b+x)^c + d

            if ((cal->b + x) <= 0)
            {
                return NAN;
            }

            y =
                cal->a * powf(cal->b + x, cal->c) +
                cal->d;

            break;


        case 4:
            // Log:
            // y = a * ln(b*x+c) + d

            if ((cal->b * x + cal->c) <= 0)
            {
                return NAN;
            }

            y =
                cal->a * logf(cal->b * x + cal->c) +
                cal->d;

            break;


        default:
            // unknown calibration
            y = x;
            break;
    }
    return y;
}