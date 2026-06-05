// RF.cpp
#include "RF.h"

RF::RF(int pin, volatile bool *intrFlag) : SensorBase(pin, intrFlag)
{
  sensitivityAdjustable = true;
  setSensitivity(50);
  rf_updateTokens();
  
}

bool RF::rf_updateTokens()
{
  sendMsgToPi("reading");

  // Guardrail: UART Timeout
  unsigned long waitStart = millis();
  while (!Serial2.available())
  {
    if (millis() - waitStart > 500)
    {
      rfDataTokens[0] = "";
      return false;
    }
  }

  String rfData = Serial2.readStringUntil('~');
  while (Serial2.available())
    Serial2.read();

  int tokenPos = 0;
  String token = "";
  for (char curChar : rfData)
  {
    if (curChar == ',')
    {
      if (tokenPos < 5) rfDataTokens[tokenPos++] = token;
      token = "";
    }
    else
    {
      token += curChar;
    }
  }
  if (tokenPos < 6) rfDataTokens[tokenPos] = token;

  // Update internal clock automatically
  if (rfDataTokens[0] != "")
  {
    clock[0] = rfDataTokens[0].substring(0, 2).toInt();
    clock[1] = rfDataTokens[0].substring(3, 5).toInt();
    clock[2] = rfDataTokens[0].substring(6).toInt();
  }

  return true;
}

float RF::getReading()
{
  rf_updateTokens();

  if (rfDataTokens[0] == "")
  {
    return INVALID_RF_READING; // INVALID_RF_READING
  }

  float readingSum = 0;
  for (int i = 1; i < 5; i++) // Fixed index bounds
  {
    readingSum += rfDataTokens[i].toFloat();
  }
  return readingSum / 4.0;
}

bool RF::calibrate()
{
  long count = 0;
  float reading = 0;

  unsigned long start_time = millis();
  unsigned long end_time = start_time;

  while (end_time - start_time <= 2000)
  {
    float rawReading = getReading();
    if (rawReading == INVALID_RF_READING)
      return false;
    reading += rawReading;
    count++;
    end_time = millis();
  }

  noiseFloor = reading / count;
  trigMin = noiseFloor * 1.2;
  trigMax = noiseFloor * 10;
  sensitivityStep = (trigMax - trigMin) / 100;
  return true;
}

bool RF::getTime(int timeComponentsArr[3])
{
  if (!rf_updateTokens()) return false;

  for (int i = 0; i < NUM_OF_TIME_COMPONENTS; i++) timeComponentsArr[i] = clock[i];
  
  return true;
}

void RF::sendMsgToPi(String msg)
{
  msg += "~";
  Serial2.print(msg);
}