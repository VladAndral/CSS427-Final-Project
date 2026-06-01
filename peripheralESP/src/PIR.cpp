// PIR.cpp
#include "PIR.h"

PIR::PIR(int pin, volatile bool* intrFlag) : SensorBase(pin, intrFlag) 
{
    pinMode(pin_num, INPUT_PULLDOWN);
    sensitivityAdjustable = false;
}

float PIR::getReading()
{
    // Simple digital read. Returns 1.0 or 0.0
    return (float)digitalRead(pin_num);
}

bool PIR::calibrate()
{
    return true; // PIR requires no calibration
}