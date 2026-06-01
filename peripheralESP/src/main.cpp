#include "peri_includes.h"
#include "PIR.h"
#include "Photo.h"
#include "RF.h"
#include "SensorSystem.h"

// ISRs (Kept in IRAM)
void IRAM_ATTR pir_ISR() { pir_intr = true; }
void IRAM_ATTR photo_ISR() { photo_intr = true; }
void IRAM_ATTR rf_ISR() { rf_intr = true; }

// --- Sensor Instantiation & Array ---
PIR pirSensor(PIR_BOARD_PIN, &pir_intr);
Photo photoSensor(PHOTO_BOARD_PIN, &photo_intr);
RF rfSensor(RF_BOARD_PIN, &rf_intr);

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
  Peri_Msg toReturn = {0}; 
  
  // Set only the specific fields that shouldn't be 0
  toReturn.recv_msg_error = true;

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
    if (target == SENSOR_PIR)
      msg_to_ctrlr.pir_data = (int)val;
    if (target == SENSOR_PHOTO)
      msg_to_ctrlr.photo_data = (int)val;
    if (target == SENSOR_RF)
      msg_to_ctrlr.rf_data = (int)val;
    return true;
  }
  else if (target == TARGET_SYSTEM)
  {
    // System demand iterates over everything
    msg_to_ctrlr.pir_data = (int)sensorArr[SENSOR_PIR]->getReading();
    msg_to_ctrlr.photo_data = (int)sensorArr[SENSOR_PHOTO]->getReading();
    msg_to_ctrlr.rf_data = (int)sensorArr[SENSOR_RF]->getReading();
    return true;
  }
  return false;
}

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
      msg_to_ctrlr.getResult = sensorArr[target]->getPollPeriod();
      return true;
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY && sensorArr[target]->isSensitivityAdjustable())
    {
      msg_to_ctrlr.getResult =  sensorArr[target]->getSensitivity();
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
  attachInterrupt(digitalPinToInterrupt(PHOTO_BOARD_PIN), photo_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(RF_BOARD_PIN), rf_ISR, CHANGE);


  executeAction[ACTION_DEMAND] = cmd_demand;
  executeAction[ACTION_SET] = cmd_set;

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

  bool pir_tripped = sensorArr[SENSOR_PIR]->checkAndClearInterrupt();
  bool photo_tripped = sensorArr[SENSOR_PHOTO]->checkAndClearInterrupt();
  bool rf_tripped = sensorArr[SENSOR_RF]->checkAndClearInterrupt();

  if (pir_tripped) { pir_intr_count++; Serial.println("tripped"); }
  if (photo_tripped) photo_intr_count++;
  if (rf_tripped) rf_intr_count++;

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
      if (i == SENSOR_PIR)
      {
        msg_to_ctrlr_polling.pir_data = (int)readingVal;
        msg_to_ctrlr_polling.pir_numOfDetct_inPeriod = pir_intr_count;
        pir_intr_count = 0;
      }
      if (i == SENSOR_PHOTO)
      {
        msg_to_ctrlr_polling.photo_data = (int)readingVal;
        msg_to_ctrlr_polling.photo_numOfDetct_inPeriod = photo_intr_count;
        photo_intr_count = 0;
      }
      if (i == SENSOR_RF)
      {
        msg_to_ctrlr_polling.rf_data = (int)readingVal;
        msg_to_ctrlr_polling.rf_numOfDetct_inPeriod = rf_intr_count;
        rf_intr_count = 0;
      }
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
    if (pir_tripped)
    {
      msg_to_ctrlr_intr.pir_detected = anyDetection = true;
    }
    if (photo_tripped)
    {
      msg_to_ctrlr_intr.photo_detected = anyDetection = true;
    }
    if (rf_tripped)
    {
      msg_to_ctrlr_intr.rf_detected = anyDetection = true;
    }

    if (anyDetection)
      sendMsgStructToController(msg_to_ctrlr_intr);
  }
}