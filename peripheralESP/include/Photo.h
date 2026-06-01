// Photo.h
#pragma once
#include "SensorBase.h"

class Photo : public SensorBase
{
private:
    float sensitivityStep;
    float noiseFloor;
public:
    Photo(int pin, volatile bool* intrFlag);
    virtual float getReading() override;
    virtual bool calibrate() override;
};