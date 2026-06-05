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

    [<sensor>/system]        [set/get/schedule]       [pollRate/sensitivity/Mode]      [<Mode>/<uint>] <uint> <uint>
    [<sensor>/system]              demand
    System behavior is autonomous

    // TODO:
      - Schedule
      - RasPi interrupt triggering
      - Network autoscan?
      - Network jamming?
      - Add time data to Peri_Msg
      - Get seconds from HackRF time
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

#define TARGET_NAMES_COUNT TARGET_SENTINEL-1
enum Target : int
{
  TARGET_INVALID,
  SENSOR_PIR,
  SENSOR_PHOTO,
  SENSOR_RF,
  TARGET_SYSTEM,
  TARGET_SENTINEL
};

#define ACTION_NAMES_COUNT ACTION_SENTINEL-1
// same thing here, can delete all the numbers because enum does the same thing
enum Action : int
{
  ACTION_INVALID,
  ACTION_DEMAND,
  ACTION_SET,
  ACTION_GET,
  ACTION_SCHEDULE,
  ACTION_SENTINEL
};

/*
    Because attribute names from all possible targets are together, just accounting for
    the offset where the next target is
*/

#define ATTR_NAME_OFFSET_SNS 0
#define ATTR_NAME_OFFSET_SYS 2


#define ATTR_NAMES_COUNT ATTR_NAME_SENTINEL-1
enum Attr_Name : int
{
  ATTR_NAME_INVALID,
  ATTR_NAME_POLL_PERIOD,
  ATTR_NAME_SENSITIVITY,
  ATTR_NAME_MODE,
  ATTR_NAME_SENTINEL,
};

#define ATTR_VALS_COUNT ATTR_VAL_SENTINEL-1
#define ATTR_VALS_OFFSET_SYS 3
enum Attr_Val : int
{
  ATTR_VAL_INVALID,
  ATTR_VAL_SNS_TRIG,
  ATTR_VAL_SNS_POLL,
  ATTR_VAL_SNS_TRIGPOLL,
  ATTR_VAL_ENABLE,
  ATTR_VAL_DISABLE,
  ATTR_VAL_SYS_NORMAL,
  ATTR_VAL_SYS_MAINT,
  ATTR_VAL_SYS_QUIET,
  ATTR_VAL_SYS_LOCKDOWN,
  ATTR_VAL_SYS_CUSTOM,
  ATTR_VAL_SENTINEL
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
  Target target = TARGET_INVALID;
  Action action = ACTION_INVALID;
  Attr_Name attr_name = ATTR_NAME_INVALID;
  Attr_Val attr_val = ATTR_VAL_INVALID;
  int val1 = -1; // Can hold pollRate, sensitivity, etc.
  int val2 = -1;
  int val3 = -1;
};

/*
    A message from the peripheral will contain all data, even if only data
    from one sensor is set
*/
#define NUM_OF_TIME_COMPONENTS 3
// Don't need to do typedef in C++ b/c it's done automatically
struct __attribute__((packed)) Peri_Msg
{
  bool recv_msg_error = true;
  
  // Works b/c _INVALID is 0
  ReadingType sensorReadingType[NUM_OF_SENSORS + INVALID_OFFSET] = {READING_INVALID};

  // Works b/c init to 0 and 0 is false
  bool sensorEnabled[NUM_OF_SENSORS + INVALID_OFFSET] = {0};
  // TODO: This does not initialize every element to -1. Must loop through and initialize
  int sensorData[NUM_OF_SENSORS + INVALID_OFFSET] = {-1};
  bool sensorDetected[NUM_OF_SENSORS + INVALID_OFFSET] = {0};
  int numOfDetectInPeriod[NUM_OF_SENSORS + INVALID_OFFSET] = {-1};

  int getResult[NUM_OF_TARGETS + INVALID_OFFSET] = {-1};

  int time[NUM_OF_TIME_COMPONENTS] = {-1};

};

// these exist and are defined in a different file - used for human translation
extern String targetNames[TARGET_NAMES_COUNT];
extern String actionNames[ACTION_NAMES_COUNT];
extern String AttributeNames[ATTR_NAMES_COUNT];
extern String AttributeValues[ATTR_VALS_COUNT];

void tokenize(String str, String arr[], int size);
void init_PeriMsg(Peri_Msg &msg);
