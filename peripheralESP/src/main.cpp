/*
 * "THE BEER-WARE LICENSE" (Revision 42):
 * regenbogencode@gmail.com wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return
 */
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

volatile bool pirDetected = false;
volatile bool photoDetected = false;
volatile bool rfDetected = false;

bool prevDetection = false;

long noiseFloor = 0;
int noiseLimit = 5;
int pollFreqPhoto = 5;
int pollTimePhoto = 250;
int photoData;

// Should not be const b/c trigMin depends on calibrated noise floor
int trigMin_photo = 100;
int trigMin_rf = 100;

/*
  Set when calibrate methods are called
*/
int trigMax_photo;
int trigMax_rf;
int sensitivityStep_photo;
int sensitivityStep_rf;

const int normalPollPeriod_ms = 50;
const int maintPollPeriod_ms = 1*1000;

long start_time;
long end_time;

int64_t timeSinceBoot;

uint32_t curTime;

int clock_hour;
int clock_minute;

bool interruptsEnabled = false;

const char *recv_data;
bool receivedData = false;

String rfDataTokens[RF_TOKEN_COUNT] = {""};

Ctrlr_Msg msg_from_ctrlr;

struct pir_struct
{
  bool enabled;
  uint32_t periodLen_ms;
  int numOfReadingsDuringPeriod;
  uint32_t prevTime;
  int data;
};

pir_struct pir;

struct photo_struct
{
  bool enabled;
  uint32_t periodLen_ms;
  uint32_t prevTime;
  int numOfReadingsDuringPeriod;
  int noiseFloor;
  int trigMin;
  uint16_t data;

  int schedule_hour;
  int schedule_minute;
  Attr_Name scheduledAttribute;
  int scheduledAttrVal;
};

photo_struct photo;

struct rf_struct
{
  bool enabled;
  uint32_t periodLen_ms;
  uint32_t prevTime;
  int numOfReadingsDuringPeriod;
  int noiseFloor;
  int trigMin;
  float data;

  int schedule_hour;
  int schedule_minute;
  Attr_Name scheduledAttribute;
  int scheduledAttrVal;
};

rf_struct rf;

struct system_struct
{
  pir_struct &pir;
  photo_struct &photo;
  rf_struct &rf;
  Attr_Val mode;

  int schedule_hour;
  int schedule_minute;
  Attr_Name scheduledAttribute;
  // Mode can be put into this variable, so it's easier to interpret
  int scheduledAttrVal;
};

bool pir_intr = false;
bool photo_intr = false;
bool rf_intr = false;

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

// Function that runs if I receive something
void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  receivedData = true;
  memcpy(&msg_from_ctrlr, data, sizeof(msg_from_ctrlr));
  // should we be checking here the size of the sent vs the recieved data? to make sure we got correct data?
}

// Function that runs if I send something
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("^ Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void IRAM_ATTR pir_ISR()
{
  pir_intr = true;
  pir.data = digitalRead(PIR_BOARD_PIN);
}

void IRAM_ATTR photo_ISR()
{
  photo_intr = true;
  photo.data = digitalRead(PHOTO_BOARD_PIN);
}

void IRAM_ATTR rf_ISR()
{
  rf_intr = true;
  // If HIGH, traffic detected
  // If LOW, no traffic detected
}

void calibratePhoto()
{
  long count = 0;
  long reading = 0;

  while (end_time - start_time <= 2000)
  {
    reading += analogRead(PHOTO_BOARD_PIN);
    count++;
    end_time = millis();
  }
  long noiseFloor = reading / count;
  trigMin_photo = noiseFloor * 1.2;
  trigMax_photo = noiseFloor * 10;
  sensitivityStep_photo = (trigMax_photo - trigMin_photo)/100;
}

// TODO:
void calibrateRF()
{
}

// TODO: Restructure read functions to only read; they shouldn't care about msg
bool readPhoto()
{
  uint16_t toStore = analogRead(PHOTO_BOARD_PIN);
  // How we enter critical sections. Do not use cli() or sei()
  portENTER_CRITICAL(&mux);
  photo.data = toStore;
  portEXIT_CRITICAL(&mux);
  return true;
}

bool readPIR()
{
  int toStore = digitalRead(PIR_BOARD_PIN);
  portENTER_CRITICAL(&mux);
  pir.data = toStore;
  portEXIT_CRITICAL(&mux);
  return true;
}

bool readRF()
{
  float readingSum = 0;

  for (int i = 1; i < rfDataTokens->length()-1; i++)
  {
    readingSum += rfDataTokens[i].toFloat();
  }

  float avgReading = readingSum / rfDataTokens->length()-1;
  rf.data = avgReading;

  return true;
}

bool readAllSensors()
{
  return readPhoto() && readPIR() && readRF();
}

void sendMsgStructToController(Peri_Msg &msg_to_ctrlr)
{
  ESPNow.send_message(controller_mac, (uint8_t *)&msg_to_ctrlr, sizeof(msg_to_ctrlr));
}

bool cmd_demand(Peri_Msg &msg_to_ctrlr)
{
  bool success = readSensor[msg_from_ctrlr.target]();

  if (success)
  {
    msg_to_ctrlr.readingType = READING_DEMAND; // Mark as on-demand report

    if (msg_from_ctrlr.target == SENSOR_PIR)
    {
      msg_to_ctrlr.pir_data = pir.data;
    }
    else if (msg_from_ctrlr.target == SENSOR_PHOTO)
    {
      msg_to_ctrlr.photo_data = photo.data;
    }
    else if (msg_from_ctrlr.target == SENSOR_RF)
    {
      msg_to_ctrlr.rf_data = rf.data;
    }
    else if (msg_from_ctrlr.target == TARGET_SYSTEM)
    {
      // Pack all data for a system-wide demand
      msg_to_ctrlr.pir_data = pir.data;
      msg_to_ctrlr.photo_data = photo.data;
      msg_to_ctrlr.rf_data = rf.data;
    }
  }
  return success;
}

bool cmd_set(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
  {
    msg_to_ctrlr.recv_msg_error = true;
    return false;
  }

  // --- PIR SENSOR ---
  if (msg_from_ctrlr.target == SENSOR_PIR)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      pir.periodLen_ms = msg_from_ctrlr.val1;
      return true;
    }
    return false;
  }
  // --- PHOTO SENSOR ---
  else if (msg_from_ctrlr.target == SENSOR_PHOTO)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      photo.periodLen_ms = msg_from_ctrlr.val1;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      photo.trigMin = trigMin_photo + msg_from_ctrlr.val1;
      return true;
    }
    return false;
  }
  // --- RF SENSOR ---
  else if (msg_from_ctrlr.target == SENSOR_RF)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      rf.periodLen_ms = msg_from_ctrlr.val1;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      rf.trigMin = trigMin_rf - msg_from_ctrlr.val1;
      return true;
    }
    return false;
  }
  // --- TARGET SYSTEM ---
  else if (msg_from_ctrlr.target == TARGET_SYSTEM)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      // Change all poll freq for all sensors
      pir.periodLen_ms = msg_from_ctrlr.val1;
      photo.periodLen_ms = msg_from_ctrlr.val1;
      rf.periodLen_ms = msg_from_ctrlr.val1;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      // PIR NOT POSSIBLE
      photo.trigMin = trigMin_photo - msg_from_ctrlr.val1;
      rf.trigMin = trigMin_rf - msg_from_ctrlr.val1;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_NORMAL)
      {
        pir.enabled = photo.enabled = rf.enabled = true;
        pir.periodLen_ms = photo.periodLen_ms = rf.periodLen_ms = normalPollPeriod_ms;
      }
      else if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_MAINT)
      {
        pir.enabled = false;
        photo.enabled = false;
        rf.enabled = true;
        rf.periodLen_ms = maintPollPeriod_ms;
      }
      else if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_QUIET)
      {
        pir.enabled = photo.enabled = rf.enabled = true;
        pir.periodLen_ms = photo.periodLen_ms = rf.periodLen_ms = normalPollPeriod_ms;
        photo.trigMin = trigMin_photo;
        rf.trigMin = trigMin_rf;
      }
      else if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_LOCKDOWN)
      {
        pir.enabled = photo.enabled = rf.enabled = false;
        interruptsEnabled = true;
      }
      return true;
    }
    return false;
  }

  return false;
}

bool cmd_get(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
  {
    msg_to_ctrlr.recv_msg_error = true;
    return false;
  }

  // --- PIR SENSOR ---
  if (msg_from_ctrlr.target == SENSOR_PIR)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      // UNCOMMENT THIS once you add 'int pir_data;' to your utils.h struct!
      msg_to_ctrlr.pir_data = pir.periodLen_ms;
      return true;
    }
  }
  // --- PHOTO SENSOR ---
  else if (msg_from_ctrlr.target == SENSOR_PHOTO)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      msg_to_ctrlr.photo_data = photo.periodLen_ms;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      msg_to_ctrlr.photo_data = photo.trigMin;
      return true;
    }
  }
  // --- RF SENSOR ---
  else if (msg_from_ctrlr.target == SENSOR_RF)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      msg_to_ctrlr.rf_data = rf.periodLen_ms;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      msg_to_ctrlr.rf_data = rf.trigMin;
      return true;
    }
  }
  // --- TARGET SYSTEM ---
  else if (msg_from_ctrlr.target == TARGET_SYSTEM)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      // Return all three poll rates
      msg_to_ctrlr.pir_data = pir.periodLen_ms; // Uncomment when added to struct
      msg_to_ctrlr.photo_data = photo.periodLen_ms;
      msg_to_ctrlr.rf_data = rf.periodLen_ms;
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      // Send back the two relevant sensitivities
      msg_to_ctrlr.photo_data = photo.trigMin;
      msg_to_ctrlr.rf_data = rf.trigMin;
      return true;
    }
  }

  return false;
}

bool cmd_schedule(Peri_Msg &msg_to_ctrlr)
{

  return false;
}

Peri_Msg new_msg_to_ctrlr()
{
  Peri_Msg toReturn;
  toReturn.pir = false;
  toReturn.photo = false;
  toReturn.rf = false;
  toReturn.recv_msg_error = true;
  toReturn.readingType = 0;
  toReturn.pir_detected = false;
  toReturn.pir_numOfDetct_inPeriod = 0;
  toReturn.photo_detected = false;
  toReturn.photo_numOfDetct_inPeriod = 0;
  toReturn.photo_data = 0;
  toReturn.rf_detected = false;
  toReturn.rf_numOfDetct_inPeriod = 0;
  toReturn.rf_data = 0;

  return toReturn;
}

bool tokenizeRFdata(String &line, String tokenArray[], int numOfTokens)
{
  if (tokenArray->length() != numOfTokens) return false;

  int tokenPos = 0;
  String token = "";
  for (char curChar : line)
  {
    if (curChar == ',')
    {
      tokenArray[tokenPos++] = token;
      token = "";
    }
    else
    {
      token += curChar;
    }
  }
  
  tokenArray[tokenPos] = token;

  return true;
}






/****************************
    SETUP AND MAIN LOOP
*****************************/

void setup()
{
  Serial.begin(9600);
  Serial2.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  Serial.println("ESPNow receiver Demo");
#ifdef ESP8266
  WiFi.mode(WIFI_STA); // MUST NOT BE WIFI_MODE_NULL
#elif ESP32
  WiFi.mode(WIFI_MODE_STA);
#endif
  WiFi.disconnect();
  esp_wifi_set_channel(PROJ_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  ESPNow.init();
  // Must add peer to send data back to it
  ESPNow.set_mac(peripheral_mac);
  ESPNow.add_peer(controller_mac);
  // Register callback functions
  ESPNow.reg_send_cb(onDataSent);
  ESPNow.reg_recv_cb(onRecv);
  // PIR stuff
  pinMode(PIR_BOARD_PIN, INPUT_PULLDOWN);

  attachInterrupt(digitalPinToInterrupt(PIR_BOARD_PIN), pir_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(PHOTO_BOARD_PIN), photo_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(RF_BOARD_PIN), rf_ISR, CHANGE);

  String timeLoggingMsg = "Times shown are relative to system bootup~";
  const char *logMsgConvert = timeLoggingMsg.c_str();
  ESPNow.send_message(controller_mac, (uint8_t *)logMsgConvert, strlen(logMsgConvert));

  readSensor[SENSOR_PIR] = readPIR;
  readSensor[SENSOR_PHOTO] = readPhoto;
  readSensor[SENSOR_RF] = readRF;
  readSensor[TARGET_SYSTEM] = readAllSensors;

  // TODO: Have command return bool if it executed properly
  executeAction[ACTION_INVALID] = emptyEA;
  executeAction[ACTION_DEMAND] = cmd_demand;
  executeAction[ACTION_SET] = cmd_set;
  executeAction[ACTION_GET] = cmd_get;
  executeAction[ACTION_SCHEDULE] = cmd_schedule;

  pir.prevTime = 0;
  photo.prevTime = 0;
  rf.prevTime = 0;

  pir.enabled = true;
  photo.enabled = true;
  rf.enabled = true;
}

// have a integer holding the ammount of times per second, if its been 1 second divided on
void loop()
{
  /*
      RasPi UART communication
  */
  Serial2.write("reading");

  if (Serial2.available())
  {
    String rfData = Serial2.readStringUntil('~');
    // Clear rest of buffer
    while (Serial2.available())
      Serial2.read();
    
    Serial.print("ESP32: I received your message: ");
    Serial.println(rfData);

    tokenizeRFdata(rfData, rfDataTokens, RF_TOKEN_COUNT);

    clock_hour = rfDataTokens[0].substring(0,2).toInt();
    clock_minute = rfDataTokens[1].substring(2).toInt();
  }

  if (Serial.available())
  {
    String userInput = Serial.readStringUntil('\n');
    userInput.trim();

    if (userInput.length())
    {
      userInput.toLowerCase();
      Serial.print("User message to pi:");
      Serial.println(userInput);

      userInput += '~';
      Serial2.print(userInput);
    }

    while (Serial.available())
      Serial.read();
  }

  if (receivedData)
  {
    Serial.println("Got something");
    receivedData = false;
    Peri_Msg msg_to_ctrlr_user = new_msg_to_ctrlr();
    if (msg_from_ctrlr.target == TARGET_INVALID)
    {
      Serial.println("Target error");
      msg_to_ctrlr_user.recv_msg_error = true;
    }
    else
    {
      Serial.println(msg_from_ctrlr.action);
      bool cmdExecuted = executeAction[msg_from_ctrlr.action](msg_to_ctrlr_user);

      Serial.println(cmdExecuted);
      msg_to_ctrlr_user.recv_msg_error = !cmdExecuted;
    }
    sendMsgStructToController(msg_to_ctrlr_user);
  }

  Peri_Msg msg_to_ctrlr_polling = new_msg_to_ctrlr();
  curTime = millis();
  bool gotReading = false;

  if (pir.enabled && (curTime - pir.prevTime) >= pir.periodLen_ms)
  {
      gotReading = true;
      readPIR();
      msg_to_ctrlr_polling.pir_data = pir.data;
      pir.prevTime = curTime;
  }

  if (photo.enabled && (curTime - photo.prevTime) >= photo.periodLen_ms)
  {
      gotReading = true;
      readPhoto();
      msg_to_ctrlr_polling.photo_data = photo.data;
      photo.prevTime = curTime;
  }

  if (rf.enabled && (curTime - rf.prevTime) >= rf.periodLen_ms)
  {
      gotReading = true;
      readRF();
      msg_to_ctrlr_polling.rf_data = rf.data;
      rf.prevTime = curTime;
  }

  if (gotReading)
  {
    msg_to_ctrlr_polling.readingType = READING_POLL;
    sendMsgStructToController(msg_to_ctrlr_polling);
  }

  if (interruptsEnabled)
  {
    Peri_Msg msg_to_ctrlr_intr = new_msg_to_ctrlr();
    bool anyDetection = false;
    
    if (pir_intr) msg_to_ctrlr_intr.pir_detected = anyDetection = true;
    if (photo_intr) msg_to_ctrlr_intr.photo_detected = anyDetection = true;
    if (rf_intr) msg_to_ctrlr_intr.pir_detected = anyDetection = true;

    msg_to_ctrlr_intr.recv_msg_error = false;

    if (anyDetection) sendMsgStructToController(msg_to_ctrlr_intr);
  }
}