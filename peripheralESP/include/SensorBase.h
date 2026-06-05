
/*
    #pragma once is the modern alternative and equivalent to
    #ifndef CLASSNAME_H
    #define CLASSNAME_H
    // class definition here
    #endif
*/
#pragma once
#include <Arduino.h>
#include <utils.h>

class SensorBase
{
protected:
  bool calibrationError;
  bool sensorEnabled;
  bool pollEnabled;
  bool sendOnIntr;
  ReadingType readingMode;
  uint32_t pollPeriodLen_ms;

  uint32_t debouncePeriodLen_ms;
  const uint32_t min_debouncePeriodLen_ms = 0;
  const uint32_t max_debouncePeriodLen_ms = 1000;
  const uint32_t sensitivityStep = (max_debouncePeriodLen_ms-min_debouncePeriodLen_ms)/100;
  int sensitivityLevel;

  unsigned long prevPollTime;
  unsigned long prevIntrTime;

  int pin_num;
  float trigMin;
  float trigMax;
  bool sensitivityAdjustable;
  volatile bool *interruptFlag; // Event flag for safe ISR handling

  bool scheduleActive[ATTR_NAME_SENTINEL];
  int schedule_hour[ATTR_NAME_SENTINEL];
  int schedule_minute[ATTR_NAME_SENTINEL];
  int scheduledAttrVal[ATTR_NAME_SENTINEL];

public:
  SensorBase(int pin, volatile bool *intrFlag);
  virtual ~SensorBase();

  // Virtual interfaces that derived classes MUST implement
  virtual bool calibrate() = 0;
  virtual float getReading() = 0;

  // Standardized polling method
  bool poll(uint32_t curTime, float &outReading);
  bool checkAndClearInterrupt(uint32_t curTime);

  void setSchedule(Attr_Name attr, int val, int hour, int min);
  bool checkAndClearSchedule(Attr_Name attr, int curHour, int curMin, int &outVal);

  // Setters & Getters
  void setEnabled(bool state);
  bool isEnabled() const;
  void setPollingEnabled(bool state);
  bool isPollingEnabled() const;
  void setSendOnIntr(bool state);
  /// @brief Should you send if an interrupt is triggered
  /// @return `true` if message should be sent instantly on trigger
  bool isSendOnIntr() const;
  void setReadingType(ReadingType mode);
  ReadingType getReadingType() const;

  void setPollPeriod(uint32_t period);
  uint32_t getPollPeriod() const;
  virtual void setSensitivity(int sensitivity);
  virtual int getSensitivity() const;
  
  unsigned long getDebouncePeriod_ms() const;

  void setPrevIntrTime_ms(unsigned long time);
  unsigned long getPrevIntrTime_ms() const;

  bool isSensitivityAdjustable() const;
  void setCalibrationError(bool isError);
  /// @brief Returns true if there was an error calibrating
  /// @return True if there was an error calibrating
  bool getCalibrationError() const; 
};