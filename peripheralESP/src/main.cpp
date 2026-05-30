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

#define PIR_DATA_ID 1
#define PHOTO_DATA_ID 2
#define RF_DATA_ID 3

volatile bool movementDetected = false;
volatile bool speechDetected = false;
volatile bool deviceDetected = false;

volatile int sensorPeriodPIR;
volatile int sensorPeriodPHOTO;
volatile int sensorPeriodRF;

bool prevDetection = false;

long noiseFloor = 0;
int noiseLimit = 5;
int pollFreqPhoto = 5;
int pollTimePhoto = 250;
int photoData;

long start_time;
long end_time;

int64_t timeSinceBoot;

uint32_t curTime;

const char *recv_data;
bool receivedData = false;

Ctrlr_Msg msg_from_ctrlr;
Peri_Msg msg_to_ctrlr;

struct PIR_struct
{
    bool enabled;
    uint32_t periodLen_ms;
    int numOfReadingsDuringPeriod;
    uint32_t prevTime;
    int data;
};

PIR_struct pir;

struct PHOTO_struct
{
    bool enabled;
    uint32_t periodLen_ms;
    uint32_t prevTime;
    int numOfReadingsDuringPeriod;
    int noiseFloor;
    int trigMin;
    uint16_t data;
};

PHOTO_struct photo;

struct RF_struct
{
    bool enabled;
    uint32_t periodLen_ms;
    uint32_t prevTime;
    int numOfReadingsDuringPeriod;
    int noiseFloor;
    int trigMin;
    float data;
};

RF_struct rf;

// Better to use this instead of Arduino's cli() and sei()
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

bool emptyRead() { return false; }
/// @brief Functions that read, and only read, their corresponding sensors
bool (*readSensor[NUM_OF_TARGETS])() = {emptyRead};

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

void pir_ISR()
{
    pir.data = digitalRead(PIR_BOARD_PIN);
}

void photo_ISR()
{
    photo.data = digitalRead(PHOTO_BOARD_PIN);
}

void rf_ISR()
{
    // If HIGH, traffic detected
    // If LOW, no traffic detected
}

// void sendMessage(int sensorTypeID)
// {
//     unsigned long timeSinceBoot = millis();
//     int seconds = (timeSinceBoot / 1000) % 60;
//     int minutes = ((timeSinceBoot / 1000) / 60) % 60;
//     int hours = ((timeSinceBoot / 1000) / 60) / 60;

//     std::string timeSinceBoot_str = "";
//     timeSinceBoot_str += std::to_string(hours);
//     timeSinceBoot_str += "hr ";
//     timeSinceBoot_str += std::to_string(minutes);
//     timeSinceBoot_str += "min ";
//     timeSinceBoot_str += std::to_string(seconds);
//     timeSinceBoot_str += "sec ";

//     const char *timeSinceBoot_chars = timeSinceBoot_str.c_str();

//     String output_str = "";
//     const char *output_chars = output_str.c_str();

//     switch (sensorTypeID)
//     {
//     case PIR_DATA_ID:
//         output_str += "PIR Sensor: Movement detected at ";
//         output_str += timeSinceBoot_chars;
//         output_str += "~";

//         output_chars = output_str.c_str();

//         Serial.println("Sending PIR sensor info to controller");
//         // NOTE: strlen does NOT count the null terminator (ASCII 0)
//         // Serial.println("\r\n    \r\n");
//         // Serial.println(charSentOut);
//         break;
//     default:
//         Serial.println("Can't send message because idk what this sensor is...");
//     }

//     ESPNow.send_message(controller_mac, (uint8_t *)output_chars, strlen(output_chars));
// }

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
}

// TODO:
void calibrateRF()
{
}

// TODO: Restructure read functions to only read; they shouldn't care about msg
bool readPhoto()
{
    // How we enter critical sections. Do not use cli() or sei()
    portENTER_CRITICAL(&mux);
    photo.data = analogRead(PHOTO_BOARD_PIN);
    portEXIT_CRITICAL(&mux);
    return true;
}

bool readPIR()
{
    portENTER_CRITICAL(&mux);
    pir.data = digitalRead(PIR_BOARD_PIN);
    portEXIT_CRITICAL(&mux);
    return true;
}

bool readRF()
{
    return false;
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
    return readSensor[msg_from_ctrlr.target]();
}
const int rfT_min = 100;
const int PhotoT_min = 100;

const int normalMode = 50;
const int maintMode = 70;
const int quietMode = 10;
const int lockdownMode = 90;

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
            photo.trigMin = PhotoT_min - msg_from_ctrlr.val1;
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
            rf.trigMin = rfT_min - msg_from_ctrlr.val1;
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
            photo.trigMin = PhotoT_min - msg_from_ctrlr.val1;
            rf.trigMin = rfT_min - msg_from_ctrlr.val1;
            return true;
        }
        else if (msg_from_ctrlr.attr_name == ATTR_NAME_MODE)
        {
            if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_NORMAL)
            {
                pir.periodLen_ms = normalMode;
                photo.periodLen_ms = normalMode;
                rf.periodLen_ms = normalMode;
            }
            else if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_MAINT)
            {
                pir.periodLen_ms = maintMode;
                photo.periodLen_ms = maintMode;
                rf.periodLen_ms = maintMode;
            }
            else if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_QUIET)
            {
                pir.periodLen_ms = quietMode;
                photo.periodLen_ms = quietMode;
                rf.periodLen_ms = quietMode;
            }
            else if (msg_from_ctrlr.attr_val == ATTR_VAL_SYS_LOCKDOWN)
            {
                pir.periodLen_ms = lockdownMode;
                photo.periodLen_ms = lockdownMode;
                rf.periodLen_ms = lockdownMode;
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
            // UNCOMMENT THIS once you add 'int PIR_data;' to your utils.h struct!
            msg_to_ctrlr.PIR_data = pir.periodLen_ms;
            return true;
        }
    }
    // --- PHOTO SENSOR ---
    else if (msg_from_ctrlr.target == SENSOR_PHOTO)
    {
        if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
        {
            msg_to_ctrlr.Photo_data = photo.periodLen_ms;
            return true;
        }
        else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
        {
            msg_to_ctrlr.Photo_data = photo.trigMin;
            return true;
        }
    }
    // --- RF SENSOR ---
    else if (msg_from_ctrlr.target == SENSOR_RF)
    {
        if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
        {
            msg_to_ctrlr.RF_data = rf.periodLen_ms;
            return true;
        }
        else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
        {
            msg_to_ctrlr.RF_data = rf.trigMin;
            return true;
        }
    }
    // --- TARGET SYSTEM ---
    else if (msg_from_ctrlr.target == TARGET_SYSTEM)
    {
        if (msg_from_ctrlr.attr_name == ATTR_NAME_POLL_FREQ)
        {
            // Return all three poll rates
            msg_to_ctrlr.PIR_data = pir.periodLen_ms; // Uncomment when added to struct
            msg_to_ctrlr.Photo_data = photo.periodLen_ms;
            msg_to_ctrlr.RF_data = rf.periodLen_ms;
            return true;
        }
        else if (msg_from_ctrlr.attr_name == ATTR_NAME_SENSITIVITY)
        {
            // Send back the two relevant sensitivities
            msg_to_ctrlr.Photo_data = photo.trigMin;
            msg_to_ctrlr.RF_data = rf.trigMin;
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
    toReturn.PIR_detected = false;
    toReturn.PIR_numOfDetct_inPeriod = 0;
    toReturn.Photo_detected = false;
    toReturn.Photo_numOfDetct_inPeriod = 0;
    toReturn.Photo_data = 0;
    toReturn.RF_detected = false;
    toReturn.RF_numOfDetct_inPeriod = 0;
    toReturn.RF_data = 0;

    return toReturn;
}

/****************************
    SETUP AND MAIN LOOP
*****************************/

void setup()
{
    Serial.begin(9600);
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
}

// have a integer holding the ammount of times per second, if its been 1 second divided on
void loop()
{

    // Serial.println("in critical section");
    // bool PIRDetectVar = movementDetected;
    // bool photoVar = speechDetected;
    // bool deviceVar = deviceDetected;

    // int prev_time_PIR;
    // int cur_time;
    // int prev_time_PHOTO;
    // int prev_time_RF;

    // cur_time = millis();
    // if (cur_time - prev_time_PIR >= sensorPeriodPIR)
    // {
    //     readPIR();
    //     sendMessage(PIR_DATA_ID);
    //     prev_time_PIR = cur_time;
    // }
    // if (cur_time - prev_time_PHOTO >= sensorPeriodPHOTO)
    // {
    //     readPhoto();
    //     sendMessage(PHOTO_DATA_ID);
    //     prev_time_PHOTO = cur_time;
    // }
    // if (cur_time - prev_time_RF >= sensorPeriodRF)
    // {
    //     readPhoto();
    //     sendMessage(RF_DATA_ID);
    //     prev_time_RF = cur_time;
    // }
    // I want this main loop to run, I want ot check the value of sensor period
    //, and if the current time is greater than the last polled time - sensor period
    // if()
    // if(end_time - start_time < pollTimePhoto/pollFreqPhoto) end_time = millis();

    // readPhoto();
    // largestReading = (curReading > largestReading) ? curReading : largestReading;

    // if (PIRDetectVar)
    // {
    //     Serial.println("Movement detected, sending message to controller");
    //     sendMessage(PIR_DATA_ID);
    //     portENTER_CRITICAL(&mux);
    //     movementDetected = false;
    //     portEXIT_CRITICAL(&mux);
    // }

    // if (photoVar)
    // {
    //     if (std::abs(photoData - noiseFloor) > std::abs(noiseFloor - noiseLimit))
    //     {
    //         sendMessage(PHOTO_DATA_ID);
    //         portENTER_CRITICAL(&mux);
    //         speechDetected = false;
    //         portEXIT_CRITICAL(&mux);
    //     }
    // }

    // if (deviceVar)
    // {
    //     portENTER_CRITICAL(&mux);
    //     deviceDetected = false;
    //     portEXIT_CRITICAL(&mux);
    // }

    /*
        Serial is for RasPi UART communication
    */
    if (Serial.available())
    {
        String data = Serial.readStringUntil('~');
        Serial.print("ESP32: I received your message: ");
        Serial.println(data + "~");
    }

    if (receivedData)
    {
        receivedData = false;
        Peri_Msg msg_to_ctrlr = new_msg_to_ctrlr();
        if (msg_from_ctrlr.target == TARGET_INVALID)
        {
            msg_to_ctrlr.recv_msg_error = true;
            sendMsgStructToController(msg_to_ctrlr);
        }
        else
        {
            bool cmdExecuted = executeAction[msg_from_ctrlr.action](msg_to_ctrlr);

            if (!cmdExecuted)
            {
                msg_to_ctrlr.recv_msg_error = true;
                sendMsgStructToController(msg_to_ctrlr);
            }
            else
            {
                // TODO:
            }
        }
    }

    curTime = millis();
    for (int i = 0; i < NUM_OF_TARGETS;)
    {
    }
}