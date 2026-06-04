#include "SensorBase.h"

SensorBase::SensorBase(int pin, volatile bool *intrFlag)
{
    pin_num = pin;
    interruptFlag = intrFlag;
    sensorEnabled = true;
    periodLen_ms = 2000; // default normalPollPeriod_ms
    prevTime = 0;
    trigMin = 0;
    sensitivityAdjustable = false;
    calibrationError = true;
}

SensorBase::~SensorBase() {}

bool SensorBase::poll(uint32_t curTime, float &outReading)
{
    if (sensorEnabled && !calibrationError && (curTime - prevTime >= periodLen_ms))
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

/// @brief Does not unset pollPeriod and isSendOnIntr
/// @param state Whether readings should be taken from the sensor
void SensorBase::setEnabled(bool state) { sensorEnabled = state; }
bool SensorBase::isEnabled() const { return sensorEnabled; }

void SensorBase::setPollingEnabled(bool state) { pollEnabled = state; }
bool SensorBase::isPollingEnabled() const { return pollEnabled; }

void SensorBase::setSendOnIntr(bool state) { sendOnIntr = state; }
bool SensorBase::isSendOnIntr() const { return sendOnIntr; }

void SensorBase::setReadingType(ReadingType mode) { readingMode = mode; }
ReadingType SensorBase::getReadingType() const { return readingMode; }

void SensorBase::setPollPeriod(uint32_t period) { periodLen_ms = period; }
uint32_t SensorBase::getPollPeriod() const { return periodLen_ms; }

void SensorBase::setSensitivity(float sensitivity) { trigMin = sensitivity; }
float SensorBase::getSensitivity() const { return trigMin; }
bool SensorBase::isSensitivityAdjustable() const { return sensitivityAdjustable; }

void SensorBase::setCalibrationError(bool state) { calibrationError = state; }
bool SensorBase::getCalibrationError() const { return calibrationError; }
