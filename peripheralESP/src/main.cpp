#include "peri_includes.h"
#include "PIR.h"
#include "Photo.h"
#include "RF.h"
#include "SensorSystem.h"
#include <utils.h>

// ISRs (Kept in IRAM)
void IRAM_ATTR pir_ISR() { sns_intr_flag[SENSOR_PIR] = true; }
void IRAM_ATTR photo_ISR() { sns_intr_flag[SENSOR_PHOTO] = true; }
void IRAM_ATTR rf_ISR() { sns_intr_flag[SENSOR_RF] = true; }

// --- Sensor Instantiation & Array ---
PIR pirSensor(PIR_BOARD_PIN, &sns_intr_flag[SENSOR_PIR]);
Photo photoSensor(PHOTO_BOARD_PIN, &sns_intr_flag[SENSOR_PHOTO]);
RF rfSensor(RF_BOARD_PIN, &sns_intr_flag[SENSOR_RF]);

// `ARRAY INDEX ZERO AND LAST INDEX IS NULLPTR!!!`
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
  init_PeriMsg(toReturn);
  
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  toReturn.sensorReadingType[i] = sensorArray[i]->getReadingType();
  
  return toReturn;
}

// =========================================================================
// CATEGORY 1: MANDATORY SENSOR BEHAVIORS (9 Points)
// =========================================================================

String behaviorPIRMotionEscalation() {
  String toReturn = "";

  static unsigned long prevEscalation = millis();

  if (millis()-prevEscalation > 6000 && system_mode == ATTR_VAL_SYS_NORMAL && sns_intr_count[SENSOR_PIR]) {
    int currentRFPoll = sensorArray[SENSOR_RF]->getPollPeriod();
    if (currentRFPoll >= 200) { 
      sensorArray[SENSOR_RF]->setPollPeriod(currentRFPoll / 2);
      toReturn = "Auto-Behavior: PIR detected motion. Doubled RF scan rate!";
      prevEscalation = millis();
    }
  }

  return toReturn;
}

String behaviorLaserBreakLockdown(bool sensorTripped[]) {
  String toReturn = "";

  if (sensorTripped[SENSOR_PHOTO] && (system_mode == ATTR_VAL_SYS_QUIET || inPowerSaveMode)) {
    system_mode = ATTR_VAL_SYS_LOCKDOWN;

    lastMotionTime = millis();

    for (int i = 1; i <= NUM_OF_SENSORS; i++)
    {
      sensorArray[i]->setReadingType(READING_TRIGPOLL);
      sensorArray[i]->setSendOnIntr(true);
      sensorArray[i]->setPollPeriod(lockdownPollPeriod_ms);
      sensorArray[i]->setSensitivity(trigMin[i] + sensitivityStep[i]);
    }
    toReturn = "Laser tripped in QUIET mode! Forced LOCKDOWN mode.";
  }

  return toReturn;
}

// TODO:
String behaviorRFSpikeDefense() {
  String toReturn = "";

  static unsigned long prevSpike = millis();

  if (millis() - prevSpike > 2000)
  {
    if (sns_intr_count[SENSOR_RF] > 3) {
      sensorArray[SENSOR_PIR]->increaseSensitivityLevel(20);
      sensorArray[SENSOR_PHOTO]->increaseSensitivityLevel(20);
      toReturn = "Auto-Behavior: High RF traffic detected! Boosted PIR & Photo sensitivity.";
      prevSpike = millis(); 
    }
  }

  return toReturn;
}

// // =========================================================================
// // CATEGORY 2: OPEN-FEATURE BEHAVIORS (10 Points)
// // =========================================================================

String behaviorIdlePowerSave() {
  String toReturn = "";

  bool allDisabled = true;
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    if (sensorArray[i]->isEnabled())
    {
      allDisabled = false;
      break;
    }
  }

  if (!allDisabled &&
    millis() - lastMotionTime > 12000 &&
    sensorArray[SENSOR_PIR]->getPollPeriod() <= 3000 &&
    !inPowerSaveMode &&
    system_mode != ATTR_VAL_SYS_LOCKDOWN &&
    system_mode != ATTR_VAL_SYS_MAINT &&
    system_mode != ATTR_VAL_SYS_CUSTOM)
  {
    system_mode = ATTR_VAL_SYS_CUSTOM;
    sensorArray[SENSOR_PIR]->setPollPeriod(5000);
    sensorArray[SENSOR_PHOTO]->setPollPeriod(5000);
    sensorArray[SENSOR_RF]->setPollPeriod(5000);
    inPowerSaveMode = true;
    toReturn = "Auto-Behavior: 12 seconds of no motion. Throttled polling to save power.";
    Serial.println(toReturn);
  }
  else
  {
    inPowerSaveMode = false;
  }
  return toReturn;
}

String behaviorRecalibrate() {
  // A static variable remembers its value even after the function finishes
  // periClock[0] is Hours, periClock[1] is Minutes
  String toReturn = "";

  static bool alreadyReset = false;

  if (!(periClock[1]%5)) {
    if (!alreadyReset)
    {
      alreadyReset = true;
      // Serial.println("Auto-Behavior: Five-minute reset. Recalibrating sensor.");
      sendLogMsgToCtrlr("Auto-Behavior: Five-minute reset. Recalibrating sensor.");
      toReturn = "Recalibration: ";
      
      for (int i = 1; i <= NUM_OF_SENSORS; i++)
      {
        sensorArray[i]->setPollPeriod(2000);
        toReturn += targetNames[i-1];
        toReturn += ": ";
        toReturn += sensorArray[i]->calibrate() ? "Passed; " : "Failed; ";
      }
      lastMotionTime = millis();
    }
  }
  else
  {
    alreadyReset = false;
  }
  return toReturn;
}

String behaviorLEDMultipleEvents(int sensorTrippedCount[])
{
  String toReturn = "";
  static unsigned long ledTurnOnTime = 0;
  static bool ledIsOn = false;
  static unsigned long lastClearTime = millis(); // Universal clear timer

  int sensorsTripped = 0;

  // 1. Evaluate the total state BEFORE touching the LED
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    if (sensorArray[i]->isEnabled() && sensorTrippedCount[i] > 0)
    {
      sensorsTripped++;
    }
  }

  // 2. Turn ON logic: If 2 or more sensors tripped AND the LED isn't already on
  if (sensorsTripped >= 2 && !ledIsOn)
  {
    digitalWrite(LED_BOARD_PIN, HIGH);
    ledIsOn = true;
    ledTurnOnTime = millis();
    toReturn = "Auto-Behavior: Multiple events detected. LED ON.";
  }

  // 3. The GUARANTEE: Turn OFF logic based strictly on an independent timer
  if (ledIsOn && (millis() - ledTurnOnTime >= 3000))
  {
    digitalWrite(LED_BOARD_PIN, LOW);
    ledIsOn = false;
  }

  // 4. Fix the State-Leak: Periodically clear counts for non-polling (TRIG) sensors
  // We simulate a 2-second "poll cycle" to clear the arrays so interrupts 
  // don't stack to infinity and permanently lock your behaviors.
  if (millis() - lastClearTime >= 2000)
  {
    for (int i = 1; i <= NUM_OF_SENSORS; i++)
    {
      if (sensorArray[i]->isEnabled() && !sensorArray[i]->isPollingEnabled())
      {
        sensorTrippedCount[i] = 0;
      }
    }
    lastClearTime = millis();
  }

  return toReturn;
}

String behaviorAutoResetVault(int sensorInterruptCount[]) {

  String toReturn = "";

  if (system_mode == ATTR_VAL_SYS_LOCKDOWN)
  {
    static int totalNumOfIntr = 0;
    if (millis()-lastMotionTime < 10000)
    {
      for (int i = 1; i <= NUM_OF_SENSORS; i++)
      {
        totalNumOfIntr += sensorInterruptCount[i];
      }
      if (totalNumOfIntr > 1)
      {
        totalNumOfIntr = 0;
        lastMotionTime = millis();
      }
    }
    else
    {
      
      totalNumOfIntr = 0;
      system_mode = ATTR_VAL_SYS_NORMAL;
      
      for (int i = 1; i <= NUM_OF_SENSORS; i++)
      {
        sensorArray[i]->setPollPeriod(normalPollPeriod_ms);
        sensorArray[i]->setReadingType(READING_TRIGPOLL);
        sensorArray[i]->setSendOnIntr(true);
      }
      
      toReturn = "Auto-Behavior: Vault secured and 10s clear of motion. Autonomously returning to NORMAL mode.";
      lastMotionTime = millis();
    }
  }
  
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

  if (target == SENSOR_PIR || target == TARGET_SYSTEM) lastMotionTime = millis();

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
      // Check if the value changed, OR if it was disabled, OR if polling was turned off (like in quiet mode)
      if (msg_from_ctrlr.val1 != sensorArray[curTarget]->getPollPeriod() || 
          !sensorArray[curTarget]->isEnabled() || 
          !sensorArray[curTarget]->isPollingEnabled())
      {
        system_mode = ATTR_VAL_SYS_CUSTOM;
      }
      sensorArray[curTarget]->setPollPeriod(msg_from_ctrlr.val1);
      if (target = SENSOR_PIR) lastMotionTime = millis();
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
    {
      if (sensorArray[curTarget]->isSensitivityAdjustable())
      {
        if (msg_from_ctrlr.val1 != sensorArray[curTarget]->getSensitivity())
          system_mode = ATTR_VAL_SYS_CUSTOM;
        sensorArray[curTarget]->setSensitivity(msg_from_ctrlr.val1);
      }
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      switch (msg_from_ctrlr.attr_val)
      {
      case ATTR_VAL_SNS_TRIG:
        system_mode = (sensorArray[curTarget]->getReadingType() != READING_TRIG) ? ATTR_VAL_SYS_CUSTOM : system_mode;
        sensorArray[curTarget]->setReadingType(READING_TRIG);
        sensorArray[curTarget]->setPollingEnabled(false);
        sensorArray[curTarget]->setSendOnIntr(true);
        break;
        case ATTR_VAL_SNS_POLL:
        system_mode = (sensorArray[curTarget]->getReadingType() != READING_POLL) ? ATTR_VAL_SYS_CUSTOM : system_mode;
        sensorArray[curTarget]->setReadingType(READING_POLL);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(false);
        break;
        case ATTR_VAL_SNS_TRIGPOLL:
        system_mode = (sensorArray[curTarget]->getReadingType() != READING_TRIGPOLL) ? ATTR_VAL_SYS_CUSTOM : system_mode;
        sensorArray[curTarget]->setReadingType(READING_TRIGPOLL);
        sensorArray[curTarget]->setPollingEnabled(true);
        sensorArray[curTarget]->setSendOnIntr(true);
        break;
      case ATTR_VAL_ENABLE:
        // If this sensor was prev. disabled, we are in a custom setting
        if (!sensorArray[curTarget]->isEnabled()) system_mode = ATTR_VAL_SYS_CUSTOM;
        sensorArray[curTarget]->setEnabled(true);
        break;
        case ATTR_VAL_DISABLE:
        // If this sensor was prev. enabled, we are in a custom setting
        if (sensorArray[curTarget]->isEnabled()) system_mode = ATTR_VAL_SYS_CUSTOM;
        sensorArray[curTarget]->setEnabled(false);
        break;
      case ATTR_VAL_SYS_NORMAL:
        sensorArray[curTarget]->setPollPeriod(normalPollPeriod_ms);
        sensorArray[curTarget]->setReadingType(READING_TRIGPOLL);
        sensorArray[curTarget]->setSendOnIntr(true);

        if (target == TARGET_SYSTEM)
        {
          system_mode = msg_from_ctrlr.attr_val;
        }
        else
        {
          if (system_mode != msg_from_ctrlr.attr_val)
            system_mode = ATTR_VAL_SYS_CUSTOM;
        }
        break;
      case ATTR_VAL_SYS_MAINT:
        if ((Target)curTarget != SENSOR_RF)
        {
          sensorArray[curTarget]->setEnabled(false);
        }
        else
        {
          sensorArray[curTarget]->setPollPeriod(maintPollPeriod_ms);
          sensorArray[curTarget]->setReadingType(READING_POLL);
          sensorArray[curTarget]->setSendOnIntr(false);
        }

        if (target == TARGET_SYSTEM)
        {
          system_mode = msg_from_ctrlr.attr_val;
        }
        else
        {
          if (system_mode != msg_from_ctrlr.attr_val)
            system_mode = ATTR_VAL_SYS_CUSTOM;
        }
        break;
      case ATTR_VAL_SYS_QUIET:
        sensorArray[curTarget]->setReadingType(READING_TRIG);
        sensorArray[curTarget]->setEnabled(true);
        sensorArray[curTarget]->setPollingEnabled(false);
        sensorArray[curTarget]->setSendOnIntr(true);
        sensorArray[curTarget]->setSensitivity(trigMin[curTarget] + sensitivityStep[curTarget]);

        if (target == TARGET_SYSTEM)
        {
          system_mode = msg_from_ctrlr.attr_val;
        }
        else
        {
          if (system_mode != msg_from_ctrlr.attr_val)
            system_mode = ATTR_VAL_SYS_CUSTOM;
        }
        break;
      case ATTR_VAL_SYS_LOCKDOWN:
        lastMotionTime = millis();
        sensorArray[curTarget]->setPollPeriod(lockdownPollPeriod_ms);
        sensorArray[curTarget]->setReadingType(READING_TRIGPOLL);
        sensorArray[curTarget]->setSendOnIntr(true);
        sensorArray[curTarget]->setSensitivity(trigMin[curTarget] + sensitivityStep[curTarget]);
        
        if (target == TARGET_SYSTEM)
        {
          system_mode = msg_from_ctrlr.attr_val;
        }
        else
        {
          if (system_mode != msg_from_ctrlr.attr_val)
            system_mode = ATTR_VAL_SYS_CUSTOM;
        }
        break;
      default:
        rejectionReason = "Don't know what this mode is";
        return false;
        break;
      }
    }
    // system_mode = msg_from_ctrlr.attr_val;
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
      else
      {
        if (target != TARGET_SYSTEM)
        {
          rejectionReason = "This sensor does not have adjustable sensitivity.";
          return false;
        }
      }
    }
    else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
    {
      if (target == TARGET_SYSTEM)
      {
        msg_to_ctrlr.getResult[TARGET_SYSTEM] = system_mode;
        break;
      }
      else
      {
        // If we're getting a sensor, return if it's enabled and also whether it's poll, trig, or trigpoll
        msg_to_ctrlr.sensorEnabled[curTarget] = sensorArray[curTarget]->isEnabled();
        
        ReadingType curSensorReadingType = sensorArray[curTarget]->getReadingType();
        Attr_Val curSensorReadingTypeAsAttrVal = (Attr_Val)curSensorReadingType;
        msg_to_ctrlr.getResult[curTarget] = curSensorReadingTypeAsAttrVal;
      }
    
    }
    msg_to_ctrlr.sensorReadingType[curTarget] = READING_GET;
  }

  // rejectionReason = "Somehow fell thru get";
  return true;
}

bool cmd_schedule(Peri_Msg &msg_to_ctrlr)
{
  if (msg_from_ctrlr.attr_name == ATTR_NAME_INVALID)
  {
    rejectionReason = "Invalid attribute name";
    return false;
  }

  Target target = msg_from_ctrlr.target;
  int curTarget = (target == TARGET_SYSTEM) ? 1 : target;
  int end = (target == TARGET_SYSTEM) ? NUM_OF_SENSORS : target;
  
  // Smartly pull from attr_val if it's a mode, otherwise grab val1
  int valToSchedule = (msg_from_ctrlr.attr_name == ATTR_NAME_MODE) ? msg_from_ctrlr.attr_val : msg_from_ctrlr.val1;

  for (curTarget; curTarget <= end; curTarget++)
  {
    sensorArray[curTarget]->setSchedule(
        msg_from_ctrlr.attr_name, 
        valToSchedule, 
        msg_from_ctrlr.val2, // Hour
        msg_from_ctrlr.val3  // Minute
    );
  }
  
  return true;
}

bool updateClock()
{
  return static_cast<RF*>(sensorArray[SENSOR_RF])->getTime(periClock);
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

  pinMode(PIR_BOARD_PIN, INPUT);
  pinMode(PHOTO_BOARD_PIN, INPUT);
  pinMode(RF_BOARD_PIN, INPUT_PULLDOWN);
  pinMode(LED_BOARD_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(PIR_BOARD_PIN), pir_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(PHOTO_BOARD_PIN), photo_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(RF_BOARD_PIN), rf_ISR, RISING);


  executeAction[ACTION_DEMAND] = cmd_demand;
  executeAction[ACTION_SET] = cmd_set;
  executeAction[ACTION_GET] = cmd_get;
  executeAction[ACTION_SCHEDULE] = cmd_schedule;

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
      sensorArray[i]->setPollPeriod(2000);
    }

    sendLogMsgToCtrlr(logMsg);
    delay(10);
  }

  updateClock();

  Serial.println("worked4");
  sendLogMsgToCtrlr("Peripheral now WORKING...");

  lastMotionTime = millis();
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

  clockUpdated = updateClock();

  if (clockUpdated)
  {
    int currentHour = periClock[0];
    int currentMinute = periClock[1];

    for (int i = 1; i <= NUM_OF_SENSORS; i++)
    {
      int scheduledVal;

      // Check Poll Period
      if (sensorArray[i]->checkAndClearSchedule(ATTR_NAME_POLL_PERIOD, currentHour, currentMinute, scheduledVal))
      {
        sensorArray[i]->setPollPeriod(scheduledVal);
      }

      // Check Sensitivity
      if (sensorArray[i]->checkAndClearSchedule(ATTR_NAME_SENSITIVITY, currentHour, currentMinute, scheduledVal))
      {
        if (sensorArray[i]->isSensitivityAdjustable())
          sensorArray[i]->setSensitivity(scheduledVal);
      }

      // Check Mode (Redirects safely into your existing cmd_set logic)
      if (sensorArray[i]->checkAndClearSchedule(ATTR_NAME_MODE, currentHour, currentMinute, scheduledVal))
      {
        // Temporarily hijack the global message struct to utilize your existing cmd_set infrastructure
        msg_from_ctrlr.target = (Target)i;
        msg_from_ctrlr.attr_name = ATTR_NAME_MODE;
        msg_from_ctrlr.attr_val = (Attr_Val)scheduledVal;
        
        Peri_Msg dummyMsg; // Dummy struct required for the function signature
        cmd_set(dummyMsg);
      }
    }
  }

  bool sensorTripped[NUM_OF_SENSORS + INVALID_OFFSET] = {false};

  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    bool curSensorTripped = sensorArray[i]->checkAndClearInterrupt(millis());

    if (curSensorTripped)
    {
      sensorTripped[i] = true;
      sns_intr_count[i]++;

      if (i == SENSOR_PIR) lastMotionTime = millis();
    }
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
    if (clockUpdated)
    {
      for (int i = 0; i < NUM_OF_TIME_COMPONENTS; i++) msg_to_ctrlr_user.time[i] = periClock[i];
    }
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

  if (gotReading)
  {
    if (clockUpdated)
    {
      for (int i = 0; i < NUM_OF_TIME_COMPONENTS; i++) msg_to_ctrlr_poll_pollTrig.time[i] = periClock[i];
    }
    sendDataStructToController(msg_to_ctrlr_poll_pollTrig);
  }
  // We might send an intr message also, give central time to receive message
  delay(10);

  // --- Hardware Interrupt Check ---
  Peri_Msg msg_to_ctrlr_intr = new_msg_to_ctrlr();
  msg_to_ctrlr_intr.recv_msg_error = false;  
  bool anyDetection = false;

  // Use the boolean states saved at the top of the loop!
  for (int i = 1; i <= NUM_OF_SENSORS; i++)
  {
    // If mode is trig, send message instantly
    if (sensorArray[i]->isEnabled() && sensorArray[i]->isSendOnIntr() && sensorTripped[i])
    {
      anyDetection = true;
      msg_to_ctrlr_intr.sensorDetected[i] = true;
      if (clockUpdated)
      {
        for (int i = 0; i < NUM_OF_TIME_COMPONENTS; i++) msg_to_ctrlr_intr.time[i] = periClock[i];
      }
      msg_to_ctrlr_intr.sensorReadingType[i] = READING_TRIG;
      sendDataStructToController(msg_to_ctrlr_intr);
    }
  }
  // ==========================================
  // RUN AUTONOMOUS BEHAVIORS
  // ==========================================
  String logMsg = "";

  // Serial.println("Running 1");
  logMsg = behaviorPIRMotionEscalation();
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);
  
  // Serial.println("Running 2");
  logMsg = behaviorLaserBreakLockdown(sensorTripped);
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);
  
  // Serial.println("Running 3");
  logMsg = behaviorRFSpikeDefense();
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);
  
  // Serial.println("Running 4");
  logMsg = behaviorIdlePowerSave();
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);
  
  // Serial.println("Running 5");
  logMsg = behaviorRecalibrate();
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);
  
  // Serial.println("Running 6");
  logMsg = behaviorLEDMultipleEvents(sns_intr_count);
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);

  // Serial.println("Running 7");
  logMsg = behaviorAutoResetVault(sns_intr_count);
  if (!logMsg.isEmpty()) sendLogMsgToCtrlr(logMsg);
  delay(5);

  // // TODO: Do we need this? I think we're double-sending when an interrupt happens
  // if (anyDetection)
  // {
  //   if (clockUpdated)
  //   {
  //     for (int i = 0; i < NUM_OF_TIME_COMPONENTS; i++) msg_to_ctrlr_intr.time[i] = periClock[i];
  //   }
  //   sendDataStructToController(msg_to_ctrlr_intr);
  // }
}