#include <utils.h>
#include <Arduino.h>

#include <esp_wifi.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif ESP32
#include <WiFi.h>
#endif
#include "ESPNowW.h"

// using namespace std;
/*
Green flag sticky is peripheral
*/

#define PIR_BOARD_PIN 13
#define PHOTO_BOARD_PIN 34
#define RF_BOARD_PIN 19

/*
    UART Pins for ESP32
    Tx: GPIO 1 -- Physical pin TX0, above D22
    Rx: GPIO 3 -- Physical pin RX0, below D21
*/
#define UART_RX GPIO_NUM_16
#define UART_TX GPIO_NUM_17

#define PIR_DATA_ID 1
#define PHOTO_DATA_ID 2
#define RF_DATA_ID 3

#define RF_TOKEN_COUNT 6

bool prevDetection = false;

long noiseFloor = 0;
int noiseLimit = 5;
int pollFreqPhoto = 5;
int pollTimePhoto = 250;
int photoData;

/*
  Set when calibrate methods are called
*/
// Should not be const b/c trigMin depends on calibrated noise floor
// TODO: Change to a "reasonable" value
float trigMin[NUM_OF_SENSORS + INVALID_OFFSET] = {100};
float trigMax[NUM_OF_SENSORS + INVALID_OFFSET] = {200};
float sensitivityStep[NUM_OF_SENSORS + INVALID_OFFSET] = {1};

const int normalPollPeriod_ms = 2*1000;
const int maintPollPeriod_ms = 1*1000;
const int quietPollPeriod_ms = 1*1000;

int64_t timeSinceBoot;

int periClock[NUM_OF_TIME_COMPONENTS] = {};
bool clockUpdated;

const char *recv_data;
volatile bool receivedData = false;

Attr_Val system_mode = ATTR_VAL_SYS_NORMAL;

String rfDataTokens[RF_TOKEN_COUNT] = {""};

Ctrlr_Msg msg_from_ctrlr;

String rejectionReason = "Default";

// --- Global Hardware Interrupt Flags ---
volatile bool sns_intr_flag[NUM_OF_SENSORS + INVALID_OFFSET] = {false};
int sns_intr_count[NUM_OF_SENSORS + INVALID_OFFSET] = {0};

// Better to use this instead of Arduino's cli() and sei()
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

bool emptyRead() { return false; }
/// @brief Functions that read, and only read, their corresponding sensors
bool (*readSensor[NUM_OF_TARGETS + INVALID_OFFSET])() = {emptyRead};

bool emptyEA(Peri_Msg &to_ctrlr) {
  rejectionReason = "Somehow running an emty execute";
  return false;
}
/// @brief Each target should have a function that interprets an action. Each function is indexed by its
/// corresponding enum. Returns false if any part of the message is not valid and/or the action was not executed.
/// Returns true if the command is valid; these functions will operate on sensors if the command is valid
/// @param to_ctrlr A reference to the message to be sent to the controller
bool (*executeAction[ACTION_SENTINEL])(Peri_Msg &) = {emptyEA};