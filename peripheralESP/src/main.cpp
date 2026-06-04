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
SensorBase *sensorArray[NUM_OF_TARGETS + 1] = {
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
// ... [Keep your ESP-NOW onRecv, onDataSent, and sendDataStructToController functions here] ...
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

void sendDataStructToController(Peri_Msg &msg_to_ctrlr)
{
  ESPNow.send_message(controller_mac, (uint8_t *)&msg_to_ctrlr, sizeof(msg_to_ctrlr));
}

void sendLogMsgToCtrlr(String logMsg)
{
  logMsg += "~";
  const char *logMsgConvert = logMsg.c_str();
  ESPNow.send_message(controller_mac, (uint8_t *)logMsgConvert, strlen(logMsgConvert));
}

void sendMsgToPi(String msg)
{
  msg += "~";
  Serial2.print(msg);
}

Peri_Msg new_msg_to_ctrlr()
{
  // {0} guarantees every single bit in the struct is safely zeroed out
  Peri_Msg toReturn;
  
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  toReturn.sensorReadingType[i] = sensorArray[i]->getReadingType();
  
  return toReturn;
}
/**********************************
    COMMANDS
**********************************/
bool cmd_demand(Peri_Msg &msg_to_ctrlr)
{
  Target target = msg_from_ctrlr.target;

  int curTarget = (target == TARGET_SYSTEM) ? 1 : target;
  int end = (target == TARGET_SYSTEM) ? NUM_OF_SENSORS : target;
  
  for (curTarget; curTarget <= end; curTarget++)
  {
    msg_to_ctrlr.sensorReadingType[curTarget] = READING_DEMAND;
    msg_to_ctrlr.sensorReadingType[curTarget] = READING_DEMAND;
    msg_to_ctrlr.sensorData[curTarget] = (int)sensorArray[curTarget]->getReading();
  }

  return true;
}

// TODO: Clean up b/c all targets have same attributes
/// @brief 
/// @param msg_to_ctrlr 
/// @return 
bool cmd_set(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
  {
    rejectionReason = "Invalid attribute name";
    return false;
  }
  
  Target target = msg_from_ctrlr.target;

  int curTarget = (target == TARGET_SYSTEM) ? 1 : target;
  int end = (target == TARGET_SYSTEM) ? NUM_OF_SENSORS : target;
  
  for (curTarget; curTarget <= end; curTarget++)
  {
    if (sensorArray[curTarget]->getCalibrationError())
    {
      // If it's system, skip the sensor
      // If it's specific sensor, return error
      if (target != TARGET_SYSTEM)
      {
        rejectionReason = "Cannot be set b/c of prev. calibration error";
        return false;
      }
      else continue;
    }
    
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_PERIOD)
    {
      if (msg_from_ctrlr.val1 != sensorArray[curTarget]->getPollPeriod())
        system_mode = ATTR_VAL_SYS_CUSTOM;
      sensorArray[curTarget]->setPollPeriod(msg_from_ctrlr.val1);
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      if (sensorArray[curTarget]->isSensitivityAdjustable())
        sensorArray[curTarget]->setSensitivity(msg_from_ctrlr.val1);
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      switch (msg_from_ctrlr.attr_val)
      {
      case ATTR_VAL_SNS_TRIG:
        sensorArray[curTarget]->setReadingType(READING_TRIG);
        sensorArray[curTarget]->setPollingEnabled(false);
        sensorArray[curTarget]->setSendOnIntr(true);
        break;
        case ATTR_VAL_SNS_POLL:
        sensorArray[curTarget]->setReadingType(READING_POLL);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(false);
        break;
        case ATTR_VAL_SNS_TRIGPOLL:
        sensorArray[curTarget]->setReadingType(READING_TRIGPOLL);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(true);
        break;
      case ATTR_VAL_ENABLE:
        sensorArray[curTarget]->setEnabled(true);
        break;
      case ATTR_VAL_DISABLE:
        sensorArray[curTarget]->setEnabled(false);
        break;
      case ATTR_VAL_SYS_NORMAL:
        sensorArray[curTarget]->setEnabled(true);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(true);
        sensorArray[curTarget]->setPollPeriod(normalPollPeriod_ms);
        system_mode = msg_from_ctrlr.attr_val;
        break;
      case ATTR_VAL_SYS_MAINT:
        if ((Target)curTarget != SENSOR_RF)
        {
          sensorArray[curTarget]->setEnabled(false);
        }
        else
        {
          sensorArray[curTarget]->setEnabled(true);
          sensorArray[curTarget]->setPollingEnabled(true);
          sensorArray[curTarget]->setSendOnIntr(false);
          sensorArray[curTarget]->setPollPeriod(maintPollPeriod_ms);
        }
        system_mode = msg_from_ctrlr.attr_val;
        break;
      case ATTR_VAL_SYS_QUIET:
        sensorArray[curTarget]->setEnabled(true);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(false);
        sensorArray[curTarget]->setPollPeriod(quietPollPeriod_ms);
        sensorArray[curTarget]->setSensitivity(trigMin[curTarget] + sensitivityStep[curTarget]);
        system_mode = msg_from_ctrlr.attr_val;
        break;
      case ATTR_VAL_SYS_LOCKDOWN:
        sensorArray[curTarget]->setEnabled(true);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(true);
        sensorArray[curTarget]->setPollPeriod(quietPollPeriod_ms);
        sensorArray[curTarget]->setSensitivity(trigMin[curTarget] + sensitivityStep[curTarget]);
        system_mode = msg_from_ctrlr.attr_val;
        break;
      default:
        rejectionReason = "Don't know what this mode is";
        return false;
        break;
      }
    }
    system_mode = msg_from_ctrlr.attr_val;
    msg_to_ctrlr.sensorReadingType[curTarget] = READING_INVALID;
  }
  
  return true;
}

// TODO:
// ... [Keep cmd_get and cmd_schedule similarly flattened] ...
bool cmd_get(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
  {
    rejectionReason = "Invalid attribute name";
    return false;
  }
  
  Target target = msg_from_ctrlr.target;

  int curTarget = target == TARGET_SYSTEM ? 1 : target;
  int end = target == TARGET_SYSTEM ? NUM_OF_SENSORS : target;
  
  for (curTarget; curTarget <= end; curTarget++)
  {
    if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_PERIOD)
    {
      msg_to_ctrlr.getResult[curTarget] = sensorArray[curTarget]->getPollPeriod();
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      if (sensorArray[curTarget]->isSensitivityAdjustable())
        msg_to_ctrlr.getResult[curTarget] = sensorArray[curTarget]->getSensitivity();
      }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      // For now, we're not going to get mode of an individual sensor
      if (target != TARGET_SYSTEM)
      {
        rejectionReason = "Cannot get mode of any indiv. sensor";
        return false;
      }
      msg_to_ctrlr.getResult[TARGET_SYSTEM] = system_mode;
      break;
    
    }
    msg_to_ctrlr.sensorReadingType[curTarget] = READING_GET;
  }

  // rejectionReason = "Somehow fell thru get";
  return true;
}

// TODO:
bool cmd_schedule(Peri_Msg &msg_to_ctrlr)
{
  rejectionReason = "Schedule not impl. yet";
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
  
  sensorArray[SENSOR_PIR]->setPollPeriod(2000);
  sensorArray[SENSOR_PHOTO]->setPollPeriod(2000);
  sensorArray[SENSOR_RF]->setPollPeriod(2000);

  pinMode(PIR_BOARD_PIN, INPUT);
  pinMode(PHOTO_BOARD_PIN, INPUT);
  pinMode(RF_BOARD_PIN, INPUT_PULLDOWN);

  attachInterrupt(digitalPinToInterrupt(PIR_BOARD_PIN), pir_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(PHOTO_BOARD_PIN), photo_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(RF_BOARD_PIN), rf_ISR, CHANGE);


  executeAction[ACTION_DEMAND] = cmd_demand;
  executeAction[ACTION_SET] = cmd_set;
  executeAction[ACTION_GET] = cmd_get;

  sendLogMsgToCtrlr("Peripheral is setting up...");

  // Calibrate objects
  bool calibrated = false;
  
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    String logMsg = targetNames[i-1];
    calibrated = sensorArray[i]->calibrate();
    
    if (calibrated)
    {
      logMsg += " is booted up";
    }
    else
    {
      logMsg += " is NOT booted up. DISABLING SENSOR";
    }

    sensorArray[i]->setCalibrationError(!calibrated);
    sensorArray[i]->setEnabled(calibrated);

    if (calibrated) {
      sensorArray[i]->setPollingEnabled(true);
      sensorArray[i]->setReadingType(READING_POLL);
    }

    sendLogMsgToCtrlr(logMsg);
    delay(10);
  }

  Serial.println("worked4");
  sendLogMsgToCtrlr("Peripheral now WORKING...");
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
      sendMsgToPi(userInput);
    }
  }

  bool sensorTripped[NUM_OF_SENSORS + INVALID_OFFSET] = {false};

  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    sensorTripped[i] = sensorArray[i]->checkAndClearInterrupt();
    if (sensorTripped[i])
    sns_intr_count[i]++;
  }
    
  // Process Incoming Controller Messages
  if (receivedData)
  {
    receivedData = false;
    Peri_Msg msg_to_ctrlr_user = new_msg_to_ctrlr();

    if (msg_from_ctrlr.target == TARGET_INVALID)
    {
      String logMsg = "Peripheral: Invalid target name";
      sendLogMsgToCtrlr(logMsg);
    }

    bool actionExecuted = executeAction[msg_from_ctrlr.action](msg_to_ctrlr_user);
    msg_to_ctrlr_user.recv_msg_error = !actionExecuted;
    String logMsg = "Peripheral: Got the message.";
    
    if (!actionExecuted)
    {
      logMsg += "\nAction did not execute: reason:";
      logMsg += rejectionReason;
      logMsg += "\n\n";
    }
    else
    {
      logMsg += " Action executed successfully";
    }

    sendLogMsgToCtrlr(logMsg);
    delay(10);
    sendDataStructToController(msg_to_ctrlr_user);
    delay(10);
  }

  /*
    POLLING AND INTERRUPTS
  */
  // --- Object-Oriented Polling Logic ---
  // Polling and polltrig data can use the same message
  Peri_Msg msg_to_ctrlr_poll_pollTrig = new_msg_to_ctrlr();
  msg_to_ctrlr_poll_pollTrig.recv_msg_error = false;

  uint32_t curTime = millis();
  bool gotReading = false;
  float pollReadingVal = 0;

  // Sensor polling
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    if (sensorArray[i]->isEnabled() && sensorArray[i]->isPollingEnabled() && sensorArray[i]->poll(curTime, pollReadingVal))
    {
      gotReading = true;
      msg_to_ctrlr_poll_pollTrig.sensorData[i] = (int)pollReadingVal;
      msg_to_ctrlr_poll_pollTrig.numOfDetectInPeriod[i] = sns_intr_count[i];
      sns_intr_count[i] = 0;
    }
  }

  if (gotReading) sendDataStructToController(msg_to_ctrlr_poll_pollTrig);
  // We might send an intr message also, give central time to receive message
  delay(10);

  // --- Hardware Interrupt Check ---
  Peri_Msg msg_to_ctrlr_intr = new_msg_to_ctrlr();
  msg_to_ctrlr_intr.recv_msg_error = false;  
  bool anyDetection = false;

  // Use the boolean states saved at the top of the loop!
  for (int i = 1; i < NUM_OF_SENSORS; i++)
  {
    // If mode is trig, send message instantly
    if (sensorArray[i]->isEnabled() && sensorArray[i]->isSendOnIntr() && sensorTripped[i])
    {
      anyDetection = true;
      msg_to_ctrlr_intr.sensorDetected[i] = true;
      sendDataStructToController(msg_to_ctrlr_intr);
    }
  }
    
  if (anyDetection) sendDataStructToController(msg_to_ctrlr_intr);
}