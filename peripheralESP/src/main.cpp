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

#include <iostream>
#include <string>
#include <cstring>

// using namespace std;
/*
Green flag sticky is peripheral
*/

#define PIR_PIN 13
#define PHOTO_PIN 14
#define RF_PIN 19

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

const char *recv_data;
bool receivedData = false;

Ctrlr_Msg msg_from_ctrlr;
Peri_Msg msg_to_ctrlr;

// Better to use this instead of Arduino's cli() and sei()
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Should not print b/c Rpi is connected to serial thru UART
void dummyFunc() {  }
/// @brief Array of different functions that will read the message sent from the controller. These functions should execute valid commands
void (*readMsg_funcArr[5])() = {dummyFunc};

/// @brief Functions that read, and only read, their corresponding sensors
void (*readSensor[TARGET_SNS_INDEX_LIMIT-1])(void) = {dummyFunc};

// Function that runs if I receive something
void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    // char macStr[18];
    // snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
    // mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
    // mac_addr[5]);

    // recv_data = (const char*)data;
    receivedData = true;
    // Serial.print("Last Packet Recv from: ");
    // // Serial.println(macStr);
    // // Serial.print("Last Packet Recv Data: ");
    // // if it could be a string, print as one
    // if (data[data_len - 1] == '~') {
    //     Serial.printf("%s\n", data);
    //     // for (int i = 0; i < data_len-1; i++) {
    //     //     Serial.printf("%c", data[i]);
    //     // }
    // }
    // // additionally print as hex
    // for (int i = 0; i < data_len; i++)
    // {
    //     Serial.printf("(hex) %x", data[i]);
    // }
    // Serial.println("");
    // string dataToParse = string((char*)data);
    // // Just duplicating what was received and sending it back to controller
    // // Must cast because compiler will not let you copy const data
    // // uint8_t *senderMac_copy = (uint8_t *)mac_addr;
    // // uint8_t *data_copy = (uint8_t *)data;
    // // ESPNow.send_message(senderMac_copy, data_copy, data_len);
    //[<sensor>/system] set [pollRate/sensitivity] <uint>
    // demand <sensor>

    // Directly copying the object sent to our local object
    memcpy(&msg_from_ctrlr, data, sizeof(msg_from_ctrlr));
}

// Function that runs if I send something
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("^ Last Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void motionDetectedISR()
{
    movementDetected = digitalRead(PIR_PIN);
}

void deviceDetectedISR()
{
    // If HIGH, traffic detected
    // If LOW, no traffic detected
}

void sendMessage(int sensorTypeID)
{
    unsigned long timeSinceBoot = millis();
    int seconds = (timeSinceBoot / 1000) % 60;
    int minutes = ((timeSinceBoot / 1000) / 60) % 60;
    int hours = ((timeSinceBoot / 1000) / 60) / 60;

    std::string timeSinceBoot_str = "";
    timeSinceBoot_str += std::to_string(hours);
    timeSinceBoot_str += "hr ";
    timeSinceBoot_str += std::to_string(minutes);
    timeSinceBoot_str += "min ";
    timeSinceBoot_str += std::to_string(seconds);
    timeSinceBoot_str += "sec ";

    const char *timeSinceBoot_chars = timeSinceBoot_str.c_str();

    String output_str = "";
    const char *output_chars = output_str.c_str();

    switch (sensorTypeID)
    {
    case PIR_DATA_ID:
        output_str += "PIR Sensor: Movement detected at ";
        output_str += timeSinceBoot_chars;
        output_str += "~";

        output_chars = output_str.c_str();

        Serial.println("Sending PIR sensor info to controller");
        // NOTE: strlen does NOT count the null terminator (ASCII 0)
        // Serial.println("\r\n    \r\n");
        // Serial.println(charSentOut);
        break;
    default:
        Serial.println("Can't send message because idk what this sensor is...");
    }

    ESPNow.send_message(controller_mac, (uint8_t *)output_chars, strlen(output_chars));
}

void calibratePhoto()
{
    long count = 0;
    long reading = 0;

    while (end_time - start_time <= 2000)
    {
        reading += analogRead(PHOTO_PIN);
        count++;
        end_time = millis();
    }
    long noiseFloor = reading / count;
}

void readPhoto()
{
    // int largestReading = analogRead(PHOTO_PIN);
    int data = 0;
    // How we enter critical sections
    portENTER_CRITICAL(&mux);
    data = analogRead(PHOTO_PIN);
    portEXIT_CRITICAL(&mux);
    msg_to_ctrlr.Photo_detected = data > noiseLimit;
}

void readPIR()
{
    int data;
    portENTER_CRITICAL(&mux);
    data = digitalRead(PIR_PIN);
    portEXIT_CRITICAL(&mux);
    msg_to_ctrlr.PIR_detected = data;
}

void readAllSensors()
{
    readPhoto();
    readPIR();
}


void sendMsgStructToController()
{
    ESPNow.send_message(controller_mac, (uint8_t *)&msg_to_ctrlr, sizeof(msg_to_ctrlr));
}

void demandSensorReading(Target sensor)
{
    readSensor[sensor];
    sendMsgStructToController();
}


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
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionDetectedISR, RISING);
    attachInterrupt(digitalPinToInterrupt(RF_PIN), deviceDetectedISR, CHANGE);

    String timeLoggingMsg = "Times shown are relative to system bootup~";
    const char *logMsgConvert = timeLoggingMsg.c_str();
    ESPNow.send_message(controller_mac, (uint8_t *)logMsgConvert, strlen(logMsgConvert));

    readSensor[SENSOR_PIR] = readPIR;
    readSensor[SENSOR_PHOTO] = readPhoto;
    readSensor[SENSOR_SENSORS] = readAllSensors;
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

        if (msg_from_ctrlr.target == TARGET_INVALID)
        {
            msg_to_ctrlr.recv_msg_error = true;
            sendMsgStructToController();
        }

        readMsg_funcArr[msg_from_ctrlr.target]();
    }
}
