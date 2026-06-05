// RF.h
#pragma once
#include "SensorBase.h"

const float INVALID_RF_READING = 100.0;

class RF : public SensorBase
{
private:
    float sensitivityStep;
    float noiseFloor;
    String rfDataTokens[6]; // RF_TOKEN_COUNT
    int clock[NUM_OF_TIME_COMPONENTS] = {};

    bool rf_updateTokens();

public:
    RF(int pin, volatile bool* intrFlag);
    virtual float getReading() override;
    virtual bool calibrate() override;
    
    bool getTime(int timeComponentsArr[3]);
    void sendMsgToPi(String msg);
};