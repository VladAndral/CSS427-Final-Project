// PIR.h
#pragma once
#include "SensorBase.h"

class PIR : public SensorBase
{
public:
    PIR(int pin, volatile bool* intrFlag);
    virtual float getReading() override;
    virtual bool calibrate() override;
};