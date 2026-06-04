/*
 * "THE BEER-WARE LICENSE" (Revision 42):
 * regenbogencode@gmail.com wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return
 */
#include <utils.h>
#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif ESP32
#include <WiFi.h>
#endif
#include "ESPNowW.h"
#include <esp_wifi.h>

/*
            Red flag sticky is central
*/

Peri_Msg msg_from_peri;

Target target;
Action action;
Attr_Name attr_name;
Attr_Val attr_val;

bool receivedData = false;

Attr_Name expectedGetAttribute = ATTR_NAME_INVALID;

// Callback function
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("^ Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  if (data_len == sizeof(Peri_Msg))
  {
    receivedData = true;
    memcpy(&msg_from_peri, data, sizeof(msg_from_peri));
    // Serial.println("Got data");
  }
  else
  {

    // // debug
    Serial.print("Not Data: ");
    Serial.println(sizeof(Peri_Msg));
    Serial.println(data_len);
    // If peri sent a string msg
    if (data[data_len - 1] == '~')
    {
      for (int i = 0; i < data_len - 1; i++)
        Serial.printf("%c", data[i]);
      Serial.println("");
    }
  }
}

Ctrlr_Msg newMsgToPeri()
{
  Ctrlr_Msg toReturn;
  return toReturn;
}

/// @brief Send the global message to the peripheral
void sendMsgStructToPeri(Ctrlr_Msg &msg_to_peri)
{
  ESPNow.send_message(peripheral_mac, (uint8_t *)&msg_to_peri, sizeof(msg_to_peri));
}

/*
    Clear serial entirely
*/
void flushSerial()
{
  while (Serial.available())
    Serial.read();
}

/// @brief Returns corresponding Target enum for user's input
/// @param parameter Token to interpret as a Target
/// @return Target enum object. May be _INVALID
Target getTarget(String parameter)
{
  if (parameter.isEmpty()) return TARGET_INVALID;

  for (int i = 0; i < TARGET_NAMES_COUNT; i++) {
    if (parameter == targetNames[i]) return (Target)(i + INVALID_OFFSET);
  }

  return TARGET_INVALID;
}

/// @brief Returns corresponding Action enum for user's input
/// @param parameter Token to interpret as a Action
/// @return Target enum object. May be _INVALID
Action getAction(String parameter, Target target)
{
  if (parameter.isEmpty()) return ACTION_INVALID;

  for (int i = 0; i < ACTION_NAMES_COUNT; i++) {
    if (parameter == actionNames[i]) return (Action)(i + INVALID_OFFSET);
  }

  return ACTION_INVALID;
}

/// @brief Returns corresponding Attribute Name enum for user's input
/// @param parameter Token to interpret as a Attribute Name
/// @return Target enum object. May be _INVALID
Attr_Name getAttrName(String parameter, Target target)
{
  if (parameter.isEmpty()) return ATTR_NAME_INVALID;
  
  for (int i = 0; i < ATTR_NAMES_COUNT; i++) {
    if (parameter == AttributeNames[i]) return (Attr_Name)(i + INVALID_OFFSET);
  }
  
  return ATTR_NAME_INVALID;
}

/// @brief Returns corresponding Attribute Value enum for user's input
/// @param parameter Token to interpret as a Attribute Value
/// @return Target enum object. May be _INVALID
/// @brief Returns corresponding Attribute Value enum for user's input
/// @param parameter Token to interpret as a Attribute Value
/// @return Target enum object. May be _INVALID
Attr_Val getAttrVal(String parameter, Target target)
{
  if (parameter.isEmpty()) return ATTR_VAL_INVALID;
  
  for (int i = 0; i < ATTR_VALS_COUNT; i++) {
    if (parameter == AttributeValues[i]) return (Attr_Val)(i + INVALID_OFFSET);
  }
  
  return ATTR_VAL_INVALID;
}

/// @brief Fills the message object's fields with user's inputs, interpreting as a sensor request.
/// If an input is not valid, message should not be sent. This method does not send the message
/// @param tokenArray Array of all tokens
/// @param arrPos Reference to position tracking variable
/// @return False if any part of the message is invalid (should not be sent)
bool buildMsg(String tokenArray[TOK_ARR_SIZE], int &arrPos, Ctrlr_Msg &msg_to_peri)
{
  /*
      PARSING: Action
  */
  action = getAction(tokenArray[arrPos++], target);

  if (action == ACTION_INVALID)
  {
    Serial.println("Invalid action.");
    return false;
  }

  msg_to_peri.action = action;

  if (action == ACTION_DEMAND)
  {
    return true;
  }

  /*
      PARSING: Attribute name
  */
  attr_name = getAttrName(tokenArray[arrPos++], target);

  if (attr_name == ATTR_NAME_INVALID)
  {
    Serial.println("Invalid attribute name.");
    return false;
  }

  msg_to_peri.attr_name = attr_name;

  if (action != ACTION_GET)
  {
    /*
        PARSING: Attribute value
    */
    int value = tokenArray[arrPos++].toInt();

    if (!value)
    {
      Serial.println("Invalid integer value.");
      return false;
    }

    msg_to_peri.val1 = value;

    if (action == ACTION_SCHEDULE)
    {
      int userTime = tokenArray[arrPos++].toInt();

      if (!userTime)
      {
        Serial.println("invalid schedule time");
        return false;
      }

      msg_to_peri.val1 = userTime;
    }
  }

  return true;
}

String getSensorName(Target target)
{
  String sensorName = "";
  switch (target)
  {
  case SENSOR_PIR:
    sensorName = "PIR";
    break;
  case SENSOR_PHOTO:
    sensorName = "PHOTODIODE";
    break;
  case SENSOR_RF:
    sensorName = "ANTENNA";
    break;
  default:
    sensorName = "!BAD TARGET";
    break;
  }
  return sensorName;
}

/****************************
    SETUP AND MAIN LOOP
*****************************/

void setup()
{
  Serial.begin(9600);
  Serial.println("ESPNow sender Demo");

#ifdef ESP8266
  WiFi.mode(WIFI_STA); // MUST NOT BE WIFI_MODE_NULL
#elif ESP32
  WiFi.mode(WIFI_MODE_STA);
#endif
  WiFi.disconnect();
  esp_wifi_set_channel(PROJ_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  ESPNow.init();

  // If you created a custom mac address, must use this function
  // ESPNow.set_mac(controller_mac);
  ESPNow.add_peer(peripheral_mac);
  // Must add peer to send data back to it
  ESPNow.reg_send_cb(onDataSent);
  // Register callback functions
  ESPNow.reg_recv_cb(onRecv);

  // During testing, buffer would send garbage on first message; flush it
  flushSerial();

  Ctrlr_Msg testMsg = newMsgToPeri();
  testMsg.target = TARGET_INVALID;
  // Test
  sendMsgStructToPeri(testMsg);
}

void loop()
{
  // static uint8_t a = 0;
  // delay(1000);
  // ESPNow.send_message(peripheral_mac, &a, 1);
  // // ++ operation increments the var after being used
  // Serial.println(a++);

  // If there was user input
  if (Serial.available())
  {
    Serial.print("Received user input: ");
    String userInput = Serial.readStringUntil('\n');
    Serial.println(userInput);

    String tokenArray[TOK_ARR_SIZE] = {""};
    tokenize(userInput, tokenArray, TOK_ARR_SIZE);
    // debug
    // for (String str : tokenArray)
    // {
    //     Serial.println(str);
    //     Serial.println(":");
    // }

    Ctrlr_Msg msg_to_peri = newMsgToPeri();
    /*
        PARSING: Target
    */
    int arrPos = 0;
    target = getTarget(tokenArray[arrPos++]);

    if (target == TARGET_INVALID)
    {
      Serial.println("Invalid target.");
    }
    else
    {
      msg_to_peri.target = target;

      // Call the function that corresponds to the target
      bool validMessage = buildMsg(tokenArray, arrPos, msg_to_peri);

      if (validMessage)
      {
        if (msg_to_peri.action == ACTION_GET)
        {
          expectedGetAttribute = msg_to_peri.attr_name;
        }
        Serial.println("Sending message...");
        sendMsgStructToPeri(msg_to_peri);
      }
    }
  }

  // If we got a message from the peripheral
  if (receivedData)
  {
    receivedData = false;
    if (msg_from_peri.recv_msg_error)
    {
      Serial.println("[ERROR]: Peripheral rejected the command.");
      return;
    }

    for (int i = 1; i <= NUM_OF_SENSORS; i++)
    {
      // if Sensor DEMAND
      if (msg_from_peri.sensorReadingType[i] == READING_DEMAND)
      {
        Serial.print("\n[ON-DEMAND REPORT] -> ");
          // Only display sensors that got readings
          // This way, we don't need to discriminate between sensor or system command
          if (msg_from_peri.sensorData[i] != -1)
          {
            String sensorName = getSensorName((Target)i);
            Serial.printf("%s Current Value: %d\n", sensorName, msg_from_peri.sensorData[i]);
          }
      }
      else if(msg_from_peri.sensorReadingType[i] == READING_TRIG || msg_from_peri.sensorReadingType[i] == READING_TRIGPOLL){
        if(msg_from_peri.sensorData[i] != -1){
          const char *sensorName = getSensorName((Target)i).c_str();

          if (msg_from_peri.sensorDetected[i])
          {
            Serial.printf("----------------PRESENCE DETECTED BY %s SENSOR-------------", sensorName);
          }
          else
          {
            Serial.printf("----------------NO PRESENCE DETECTED BY %s SENSOR-------------", sensorName);
          }
        }
      }
      else if(msg_from_peri.sensorReadingType[i] == READING_POLL || msg_from_peri.sensorReadingType[i] == READING_TRIGPOLL){
        if(msg_from_peri.sensorData[i] != -1){
          const char *sensorName = getSensorName((Target)i).c_str();
          Serial.printf("----------------%s WAS TRIPPED %u TIMES-------------\n\n", sensorName, msg_from_peri.numOfDetectInPeriod[i]);
          //need to print the sensor value at that time interval. 
          Serial.printf("%s data is: %d\n", sensorName, msg_from_peri.sensorData[i]);        
        }
      }
      //get reading
      else if (msg_from_peri.sensorReadingType[i] == READING_GET || expectedGetAttribute == ATTR_NAME_MODE)
      {
        if (expectedGetAttribute == ATTR_NAME_MODE)
        { 
          String mode = AttributeValues[msg_from_peri.getResult[TARGET_SYSTEM] - 1];
          Serial.printf("System's mode is: %s\n\n", mode);
          // Break out of the loop because only system will do mode
          break;
        }

        if (msg_from_peri.getResult[i] != -1)
        {
          Serial.print("\n[GET COMMAND RESULT] -> \n");
          String targetName = getSensorName((Target)i);
          
          int result = msg_from_peri.getResult[i];
          Serial.printf("%s Property is: %d\n\n", targetName, result);
        }
        else
        {
          Serial.print("\nERROR: Somehow got bad reading from get command\n");
        }
      }
    }
  } //
}