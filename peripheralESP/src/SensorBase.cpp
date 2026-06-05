#include "SensorBase.h"

SensorBase::SensorBase(int pin, volatile bool *intrFlag)
{
    pin_num = pin;
    interruptFlag = intrFlag;
    sensorEnabled = true;
    pollPeriodLen_ms = 2000; // default normalPollPeriod_ms
    setSensitivity(50);
    prevPollTime = 0;
    prevIntrTime = 0;
    trigMin = 0;
    sensitivityAdjustable = false;
    calibrationError = true;

    // Zero out schedule memory to prevent random executions
    for (int i = 0; i < ATTR_NAME_SENTINEL; i++)
    {
        scheduleActive[i] = false;
        schedule_hour[i] = -1;
        schedule_minute[i] = -1;
        scheduledAttrVal[i] = -1;
    }
}

SensorBase::~SensorBase() {}

bool SensorBase::poll(uint32_t curTime, float &outReading)
{
    if (sensorEnabled && !calibrationError && (curTime - prevPollTime >= pollPeriodLen_ms))
    {
        outReading = getReading();
        prevPollTime = curTime;
        return true;
    }
    return false;
}

bool SensorBase::checkAndClearInterrupt(uint32_t curTime)
{
    // 1. Did the hardware flag physically trigger?
    if (interruptFlag && *interruptFlag)
    {
        // 2. Immediately clear the hardware flag so we don't miss future events
        *interruptFlag = false;
        
        // 3. Evaluate the Debounce Window
        if (curTime - prevIntrTime >= debouncePeriodLen_ms)
        {
            prevIntrTime = curTime; // Reset the debounce timer!
            return true;            // Valid, debounced interrupt
        }
    }
    
    // Returns false if there was no interrupt, OR if it was bounced as noise
    return false; 
}

void SensorBase::setSchedule(Attr_Name attr, int val, int hour, int min)
{
    scheduleActive[attr] = true;
    scheduledAttrVal[attr] = val;
    schedule_hour[attr] = hour;
    schedule_minute[attr] = min;
}

bool SensorBase::checkAndClearSchedule(Attr_Name attr, int curHour, int curMin, int &outVal)
{
    // If a schedule exists and the clock perfectly matches the hour and minute
    if (scheduleActive[attr] && schedule_hour[attr] == curHour && schedule_minute[attr] == curMin)
    {
        scheduleActive[attr] = false; // Disable it so it doesn't run 60 times this minute!
        outVal = scheduledAttrVal[attr];
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

void SensorBase::setPollPeriod(uint32_t period)
{
    setEnabled(true);
    setPollingEnabled(true);
    pollPeriodLen_ms = period;

    // Automatically correct the reading type so the central parses the data
    if (readingMode == READING_TRIG) {
        setReadingType(READING_TRIGPOLL);
    } else {
        setReadingType(READING_POLL);
    }
}
uint32_t SensorBase::getPollPeriod() const { return pollPeriodLen_ms; }

void SensorBase::setSensitivity(int sensitivity)
{
    if (sensitivityAdjustable)
    {
        sensitivityLevel = sensitivity;
        int level = 100-sensitivityLevel;
        debouncePeriodLen_ms = min_debouncePeriodLen_ms + (sensitivityStep*level);
    }
    else
    {
        debouncePeriodLen_ms = min_debouncePeriodLen_ms;
    }
}

int SensorBase::getSensitivity() const { return sensitivityLevel; }

unsigned long SensorBase::getDebouncePeriod_ms() const
{
    return debouncePeriodLen_ms;
}

void SensorBase::setPrevIntrTime_ms(unsigned long time)
{
    prevIntrTime = time;
}

unsigned long SensorBase::getPrevIntrTime_ms() const
{
    return prevIntrTime;
}
bool SensorBase::isSensitivityAdjustable() const { return sensitivityAdjustable; }

void SensorBase::setCalibrationError(bool state) { calibrationError = state; }
bool SensorBase::getCalibrationError() const { return calibrationError; }
