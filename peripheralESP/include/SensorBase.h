
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
  bool enabled;
  uint32_t periodLen_ms;
  uint32_t prevTime;
  int pin_num;
  float trigMin;
  float trigMax;
  bool sensitivityAdjustable;
  volatile bool *interruptFlag; // Event flag for safe ISR handling

  // Indexed by the Attribute Name
  bool scheduleActive[ATTR_NAME_SENTINEL - 1];
  int schedule_hour[ATTR_NAME_SENTINEL - 1];
  int schedule_minute[ATTR_NAME_SENTINEL - 1];
  int scheduledAttrVal[ATTR_NAME_SENTINEL - 1];

public:
  SensorBase(int pin, volatile bool *intrFlag);
  virtual ~SensorBase();

  // Virtual interfaces that derived classes MUST implement
  virtual bool calibrate() = 0;
  virtual float getReading() = 0;

  // Standardized polling method
  bool poll(uint32_t curTime, float &outReading);
  bool checkAndClearInterrupt();

  // Setters & Getters
  void setEnabled(bool state);
  bool isEnabled() const;
  void setPollPeriod(uint32_t period);
  uint32_t getPollPeriod() const;
  virtual void setSensitivity(float sensitivity);
  virtual float getSensitivity() const;
  bool isSensitivityAdjustable() const;
};