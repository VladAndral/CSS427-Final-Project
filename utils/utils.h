#include <WString.h>

#define PROJ_WIFI_CHANNEL 11
#define TOK_ARR_SIZE 10

/*
    Might not need these
*/
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

/*
    target              action              attributeName               attributeValue

    <sensor>      [set/get/schedule]    [pollRate/sensitivity]       <uint> (<uint> <uint>)
    <sensor>            demand
    system        [set/get/schedule]            mode              <Mode> (<uint> <uint> <uint>)
*/

/*
    To abstract returning what enum type a token is

    Count is for function pointer array
*/
#define INVALID_OFFSET 1
// Where the sensors start and stop
#define TARGET_SNS_INDEX_BEGIN 1
#define TARGET_SNS_INDEX_LIMIT SENSOR_SENSORS
#define TARGET_SYS_INDEX_BEGIN TARGET_SNS_INDEX_LIMIT+1
enum Target : int
{
    TARGET_INVALID = 0,
    SENSOR_PIR = TARGET_SNS_INDEX_BEGIN,
    SENSOR_PHOTO = TARGET_SNS_INDEX_BEGIN+1,
    SENSOR_RF = TARGET_SNS_INDEX_BEGIN+2,
    SENSOR_SENSORS = TARGET_SNS_INDEX_BEGIN+3,
    TARGET_SYSTEM = TARGET_SYS_INDEX_BEGIN
};

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
    ATTR_NAME_SNS_POLL_FREQ = 1,
    ATTR_NAME_SNS_SENSITIVITY = 2,
    ATTR_NAME_SYS_MODE = 3
};
#define ATTR_VAL_OFFSET_SYS 2
enum Attr_Val : int
{
    ATTR_VAL_INVALID = 0,
    ATTR_VAL_SYS_NORMAL = 1,
    ATTR_VAL_SYS_MAINT = 2,
    ATTR_VAL_SYS_QUIET = 3,
    ATTR_VAL_SYS_LOCKDOWN = 4
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
#define VALUE_INVALID -9999
typedef struct __attribute__((packed))
{
    Target target;
    Action action;
    Attr_Name attr_name;
    Attr_Val attr_val;
    int val1; // Can hold pollRate, sensitivity, etc.
    int val2;
    int val3;
} Ctrlr_Msg;

/*
    A message from the peripheral will contain all data, even if only data
    from one sensor is set
*/
typedef struct __attribute__((packed))
{
    bool recv_msg_error;
    bool PIR_detected;
    int PIR_numOfDetct_inPeriod;
    bool Photo_detected;
    int Photo_numOfDetct_inPeriod;
    int Photo_noiseLevel;
    bool RF_detected;
    int RF_numOfDetct_inPeriod;
    int RF_noiseLevel;
} Peri_Msg;

extern String targetNames[];
extern String actionNames[];
extern String AttributeNames_sensor[];
extern String AttributeNames_system[];
extern String AttributeValues_system[];

void tokenize(String str, String arr[], int size);
