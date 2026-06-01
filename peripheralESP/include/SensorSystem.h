#pragma once
#include <Arduino.h>
#include "PIR.h"
#include "Photo.h"
#include "RF.h"

class SensorSystem
{
private:
  PIR pir;
  Photo photo;
  RF rf;
  Attr_Val mode;

  // Indexed by the Attribute Name
  bool scheduleActive[ATTR_NAME_SENTINEL-1];
  int schedule_hour[ATTR_NAME_SENTINEL-1];
  int schedule_minute[ATTR_NAME_SENTINEL-1];
  // Since attribute could be both mode enum and int val, interpret as int
  int scheduledAttrVal[ATTR_NAME_SENTINEL-1];

public:
  SensorSystem(PIR pir, Photo photo, RF rf);
  ~SensorSystem();

  // S & Gs
  void setMode(Attr_Val);
  Attr_Val getMode();

  void setSensitivity(int sensitivity);
  String getSensitivity();

  void setPollRate(int pollRate);
  String getPollRate();

};