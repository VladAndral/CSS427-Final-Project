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

#define READING_DEMAND 0
#define READING_POLL 1
#define READING_INTR 2

#define RF_TOKEN_COUNT 6

bool prevDetection = false;

long noiseFloor = 0;
int noiseLimit = 5;
int pollFreqPhoto = 5;
int pollTimePhoto = 250;
int photoData;

// Should not be const b/c trigMin depends on calibrated noise floor
int trigMin_photo = 100;
float trigMin_rf = 100;

/*
  Set when calibrate methods are called
*/
int trigMax_photo;
float trigMax_rf;
int sensitivityStep_photo;
float sensitivityStep_rf;

const int normalPollPeriod_ms = 2000;
const int maintPollPeriod_ms = 1*1000;

int64_t timeSinceBoot;

uint32_t curTime;

int clock_hour;
int clock_minute;

bool interruptsEnabled = false;

const char *recv_data;
volatile bool receivedData = false;

String rfDataTokens[RF_TOKEN_COUNT] = {""};

Ctrlr_Msg msg_from_ctrlr;

// --- Global Hardware Interrupt Flags ---
volatile bool pir_intr = false;
int pir_intr_count = 0;
volatile bool photo_intr = false;
int photo_intr_count = 0;
volatile bool rf_intr = false;
int rf_intr_count = 0;

// Better to use this instead of Arduino's cli() and sei()
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

bool emptyRead() { return false; }
/// @brief Functions that read, and only read, their corresponding sensors
bool (*readSensor[NUM_OF_TARGETS + INVALID_OFFSET])() = {emptyRead};

bool emptyEA(Peri_Msg &to_ctrlr) { return false; }
/// @brief Each target should have a function that interprets an action.
/// Returns false if any part of the message is not valid and/or the action was not executed.
/// Returns true if the command is valid; these functions will operate on sensors if the command is valid
/// @param Takes a reference to the message to be sent to the controller
bool (*executeAction[NUM_OF_TARGETS + INVALID_OFFSET])(Peri_Msg &) = {emptyEA};