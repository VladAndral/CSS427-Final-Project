#include <WString.h>

#pragma once

// find and replace
#define PROJ_WIFI_CHANNEL 11
#define TOK_ARR_SIZE 10

/*
    Might not need these
*/
// maps human readable word to a number. We choose int because 32bit processor.
enum SensorID : int
{
  ID_PIR = 1,
  ID_PHOTO = 2,
  ID_RF = 3
};

enum CommandType : int
{
  CMD_DEMAND = 1,
  CMD_SET_POLL = 2,
  CMD_SYS_MODE = 3
};

enum ReadingType : int
{
  READING_INVALID,
  READING_DEMAND,
  READING_POLL,
  READING_TRIG,
  READING_TRIGPOLL,
  READING_GET
};

/*
         target                    action                    attributeName                    attributeValue

    [<sensor>/system]        [set/get/schedule]       [pollRate/sensitivity/Mode]      <Mode> (<uint> <uint> <uint>)
    [<sensor>/system]              demand
*/

#define NUM_OF_TARGETS TARGET_SENTINEL - 1
#define NUM_OF_SENSORS 3
// we use this target_sentinal to 5 -1, to get the number of targets we can send commands to.
/*
To abstract returning what enum type a token is

Count is for function pointer array
*/

#define INVALID_OFFSET 1
// note that for the Target_sentinal to matter, we would change and not set any of these
// options to numbers, and instead leave them blank and enum would auto increment each one

enum Target : int
{
  TARGET_INVALID = 0,
  SENSOR_PIR = 1,
  SENSOR_PHOTO = 2,
  SENSOR_RF = 3,
  TARGET_SYSTEM = 4,
  TARGET_SENTINEL = 5
};

// same thing here, can delete all the numbers because enum does the same thing
enum Action : int
{
  ACTION_INVALID = 0,
  ACTION_DEMAND = 1,
  ACTION_SET = 2,
  ACTION_GET = 3,
  ACTION_SCHEDULE = 4
};

/*
    Because attribute names from all possible targets are together, just accounting for
    the offset where the next target is
*/

#define ATTR_NAME_OFFSET_SNS 0
#define ATTR_NAME_OFFSET_SYS 2

enum Attr_Name : int
{
  ATTR_NAME_INVALID = 0,
  ATTR_NAME_POLL_FREQ = 1,
  ATTR_NAME_SENSITIVITY = 2,
  ATTR_NAME_MODE = 3,
  ATTR_NAME_SENTINEL = 4
};

#define ATTR_VAL_OFFSET_SYS 3
enum Attr_Val : int
{
  ATTR_VAL_INVALID = 0,
  ATTR_VAL_SNS_TRIG = 1,
  ATTR_VAL_SNS_POLL = 2,
  ATTR_VAL_SNS_TRIGPOLL = 3,
  ATTR_VAL_SYS_NORMAL = 4,
  ATTR_VAL_SYS_MAINT = 5,
  ATTR_VAL_SYS_QUIET = 6,
  ATTR_VAL_SYS_LOCKDOWN = 7,
  ATTR_VAL_SYS_CUSTOM = 8
};

/*
    Discover built-in/default mac address by running:
        Serial.begin(####);
        WiFi.mode(WIFI_MODE_STA);
        Serial.println(WiFi.macAddress());

    You can also make your own mac address, but you must use set_mac function
*/
extern uint8_t controller_mac[];
extern uint8_t peripheral_mac[];

/*
    The central will pack the data into this struct and send to peripheral
    THe peripheral will cast the received uint...pointer back to this struct
    and use it. This way, the peripheral is not the one doing the string parsing

    Packed struct ensures consistent byte alignment across devices
*/
// we have this because if we just initialize
//  it gets set to 0 and we dont know if the sensor reading is 0 or not
// if its this value, then invalid - periferal checks if val1 = invalid, then ignore
#define VALUE_INVALID -9999
// typedef struct __attribute__((packed))
// packed tells the system to ignore all internal padding
struct __attribute__((packed)) Ctrlr_Msg
{
  Target target;
  Action action;
  Attr_Name attr_name;
  Attr_Val attr_val;
  int val1; // Can hold pollRate, sensitivity, etc.
  int val2;
  int val3;
};

/*
    A message from the peripheral will contain all data, even if only data
    from one sensor is set
*/
// Don't need to do typedef in C++ b/c it's done automatically
struct __attribute__((packed)) Peri_Msg
{
  bool recv_msg_error = true;
  
  ReadingType readingType = READING_INVALID;

  bool sensorEnabled[NUM_OF_SENSORS + INVALID_OFFSET] = {false};
  int sensorData[NUM_OF_SENSORS + INVALID_OFFSET] = {-1};
  bool sensorDetected[NUM_OF_SENSORS + INVALID_OFFSET] = {false};
  int numOfDetectInPeriod[NUM_OF_SENSORS + INVALID_OFFSET] = {-1};

  int getResult[NUM_OF_TARGETS] = {-1};

};

// these exist and are defined in a different file - used for human translation
extern String targetNames[];
extern String actionNames[];
extern String AttributeNames[];
extern String AttributeValues[];

void tokenize(String str, String arr[], int size);
