#include "SensorBase.h"

SensorBase::SensorBase(int pin, volatile bool* intrFlag)
{
    pin_num = pin;
    interruptFlag = intrFlag;
    enabled = true;
    periodLen_ms = 2000; // default normalPollPeriod_ms
    prevTime = 0;
    trigMin = 0;
    sensitivityAdjustable = false;
}

SensorBase::~SensorBase() {}

bool SensorBase::poll(uint32_t curTime, float &outReading)
{
    if (enabled && (curTime - prevTime >= periodLen_ms))
    {
        outReading = getReading();
        prevTime = curTime;
        return true;
    }
    return false;
}

bool SensorBase::checkAndClearInterrupt()
{
    if (interruptFlag && *interruptFlag)
    {
        // Atomically read and clear the flag outside the critical hardware ISR
        *interruptFlag = false;
        return true;
    }
    return false;
}

void SensorBase::setEnabled(bool state) { enabled = state; }
bool SensorBase::isEnabled() const { return enabled; }
void SensorBase::setPollPeriod(uint32_t period) { periodLen_ms = period; }
uint32_t SensorBase::getPollPeriod() const { return periodLen_ms; }
void SensorBase::setSensitivity(float sensitivity) { trigMin = sensitivity; }
float SensorBase::getSensitivity() const { return trigMin; }
bool SensorBase::isSensitivityAdjustable() const { return sensitivityAdjustable; }