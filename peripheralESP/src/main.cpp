#include "peri_includes.h"
#include "PIR.h"
#include "Photo.h"
#include "RF.h"
#include "SensorSystem.h"

// ISRs (Kept in IRAM)
void IRAM_ATTR pir_ISR() { sns_intr_flag[SENSOR_PIR] = true; }
void IRAM_ATTR photo_ISR() { sns_intr_flag[SENSOR_PHOTO] = true; }
void IRAM_ATTR rf_ISR() { sns_intr_flag[SENSOR_RF] = true; }

// --- Sensor Instantiation & Array ---
PIR pirSensor(PIR_BOARD_PIN, &sns_intr_flag[SENSOR_PIR]);
Photo photoSensor(PHOTO_BOARD_PIN, &sns_intr_flag[SENSOR_PHOTO]);
RF rfSensor(RF_BOARD_PIN, &sns_intr_flag[SENSOR_RF]);

// We size to NUM_OF_TARGETS + 1 so the enums (1 = PIR, 2 = Photo, 3 = RF) match the indices perfectly
SensorBase *sensorArr[NUM_OF_TARGETS + 1] = {
  nullptr,
  &pirSensor,
  &photoSensor,
  &rfSensor,
  nullptr
};

SensorSystem systemSensor(pirSensor, photoSensor, rfSensor);
/**********************************
    MESSAGING
**********************************/
// ... [Keep your ESP-NOW onRecv, onDataSent, and sendMsgStructToController functions here] ...
// Function that runs if I receive something
void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  receivedData = true;
  memcpy(&msg_from_ctrlr, data, sizeof(msg_from_ctrlr));
  Serial.println("Got something");
  // should we be checking here the size of the sent vs the recieved data? to make sure we got correct data?
}

// Function that runs if I send something
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("^ Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void sendMsgStructToController(Peri_Msg &msg_to_ctrlr)
{
  ESPNow.send_message(controller_mac, (uint8_t *)&msg_to_ctrlr, sizeof(msg_to_ctrlr));
}

void sendLogMsgToCtrlr(String logMsg)
{
  const char *logMsgConvert = logMsg.c_str();
  ESPNow.send_message(controller_mac, (uint8_t *)logMsgConvert, strlen(logMsgConvert));
}

Peri_Msg new_msg_to_ctrlr()
{
  // {0} guarantees every single bit in the struct is safely zeroed out
  Peri_Msg toReturn; 
  
  return toReturn;
}
/**********************************
    COMMANDS
**********************************/
bool cmd_demand(Peri_Msg &msg_to_ctrlr)
{
  Target target = msg_from_ctrlr.target;
  msg_to_ctrlr.readingType = READING_DEMAND;

  if (target >= SENSOR_PIR && target <= SENSOR_RF)
  {
    float val = sensorArr[target]->getReading();
    msg_to_ctrlr.sensorData[target] = (int)val;
    return true;
  }
  else if (target == TARGET_SYSTEM)
  {
    // System demand iterates over everything
    for (int i = 1; i <= NUM_OF_SENSORS; i++)
      msg_to_ctrlr.sensorData[i] = (int)sensorArr[i]->getReading();
    return true;
  }
  return false;
}

// TODO: Clean up b/c all targets have same attributes
/// @brief 
/// @param msg_to_ctrlr 
/// @return 
bool cmd_set(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
    return false;
  Target target = msg_from_ctrlr.target;

  if (target >= SENSOR_PIR && target <= SENSOR_RF)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      sensorArr[target]->setPollPeriod(msg_from_ctrlr.val1);
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY && sensorArr[target]->isSensitivityAdjustable())
    {
      sensorArr[target]->setSensitivity(msg_from_ctrlr.val1);
      return true;
    }
  }
  else if (target == TARGET_SYSTEM)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      for (int i = 1; i <= 3; i++)
        sensorArr[i]->setPollPeriod(msg_from_ctrlr.val1);
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      for (int i = 1; i <= 3; i++)
        sensorArr[i]->setSensitivity(msg_from_ctrlr.val1);
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      // TODO:
      // Handle system modes (Normal, Maint, Lockdown) here using your existing logic
      return true;
    }
  }
  return false;
}

// TODO:
// ... [Keep cmd_get and cmd_schedule similarly flattened] ...
bool cmd_get(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
    return false;
  Target target = msg_from_ctrlr.target;

  if (target >= SENSOR_PIR && target <= SENSOR_RF)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      msg_to_ctrlr.getResult[target] = sensorArr[target]->getPollPeriod();
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY && sensorArr[target]->isSensitivityAdjustable())
    {
      msg_to_ctrlr.getResult[target] = sensorArr[target]->getSensitivity();
      return true;
    }
  }
  else if (target == TARGET_SYSTEM)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
    {
      // TODO:
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      // TODO:
      // Handle system modes (Normal, Maint, Lockdown) here using your existing logic
      return true;
    }
  }
  return false;
}

// TODO:
bool cmd_schedule(Peri_Msg &msg_to_ctrlr)
{
  return false;
}

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
  
  sensorArr[SENSOR_PIR]->setPollPeriod(2000);
  sensorArr[SENSOR_PHOTO]->setPollPeriod(2000);
  sensorArr[SENSOR_RF]->setPollPeriod(2000);

  pinMode(PIR_BOARD_PIN, INPUT);
  pinMode(PHOTO_BOARD_PIN, INPUT_PULLDOWN);
  pinMode(RF_BOARD_PIN, INPUT_PULLDOWN);

  attachInterrupt(digitalPinToInterrupt(PIR_BOARD_PIN), pir_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(PHOTO_BOARD_PIN), photo_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(RF_BOARD_PIN), rf_ISR, CHANGE);


  executeAction[ACTION_DEMAND] = cmd_demand;
  executeAction[ACTION_SET] = cmd_set;
  executeAction[ACTION_GET] = cmd_get;

  sendLogMsgToCtrlr("Peripheral is booted up~");

  // Calibrate objects
  sensorArr[SENSOR_PHOTO]->calibrate();
  sendLogMsgToCtrlr("Photodiode sensor is booted up~");

  if (sensorArr[SENSOR_RF]->calibrate())
  {
    sendLogMsgToCtrlr("HackRF is booted up~");
  }
  else
  {
    sendLogMsgToCtrlr("ERROR: HackRF is NOT booted up.~");
    sensorArr[SENSOR_RF]->setEnabled(false);
  }
}

/*
  TODO:
  - Have RasPi trigger interrupt each time minute changes
  - Have set update system variable that tracks if system mode is set or user custom
  - Both this and controller: set sensor mode to poll and/or trig

*/
void loop()
{
  // UART relay to RasPi
  if (Serial.available())
  {
    String userInput = Serial.readStringUntil('\n'); // Cleaned up hidden char bug
    userInput.trim();
    if (userInput.length())
    {
      Serial2.print(userInput + "~");
    }
  }

  bool sensorTripped[NUM_OF_SENSORS] = {false};

  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    sensorTripped[i] = sensorArr[i]->checkAndClearInterrupt();
    if (sensorTripped[i])
    sns_intr_count[SENSOR_PIR]++;
  }
    
  // Process Incoming Controller Messages
  if (receivedData)
  {
    receivedData = false;
    Peri_Msg msg_to_ctrlr_user = new_msg_to_ctrlr();

    if (msg_from_ctrlr.target != TARGET_INVALID)
    {
      msg_to_ctrlr_user.recv_msg_error = !executeAction[msg_from_ctrlr.action](msg_to_ctrlr_user);
    }
    sendMsgStructToController(msg_to_ctrlr_user);
  }

  // --- Object-Oriented Polling Logic ---
  Peri_Msg msg_to_ctrlr_polling = new_msg_to_ctrlr();
  msg_to_ctrlr_polling.recv_msg_error = false;
  uint32_t curTime = millis();
  bool gotReading = false;
  float readingVal = 0;

  // A single loop handles all sensors dynamically!
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    if (sensorArr[i]->poll(curTime, readingVal))
    {
      gotReading = true;
      msg_to_ctrlr_polling.sensorData[msg_from_ctrlr.target] = (int)readingVal;
      msg_to_ctrlr_polling.numOfDetectInPeriod[msg_from_ctrlr.target] = sns_intr_count[msg_from_ctrlr.target];
      sns_intr_count[msg_from_ctrlr.target] = 0;
    }
  }

  if (gotReading)
  {
    msg_to_ctrlr_polling.readingType = READING_POLL;
    sendMsgStructToController(msg_to_ctrlr_polling);
  }

  // --- Hardware Interrupt Check ---
  if (interruptsEnabled)
  {
    Peri_Msg msg_to_ctrlr_intr = new_msg_to_ctrlr();
    msg_to_ctrlr_intr.recv_msg_error = false;
    bool anyDetection = false;

    // Use the boolean states saved at the top of the loop!
    for (int i = 1; i < NUM_OF_SENSORS; i++)
      msg_to_ctrlr_intr.sensorDetected[i] = anyDetection = true;

    if (anyDetection)
      sendMsgStructToController(msg_to_ctrlr_intr);
  }
}