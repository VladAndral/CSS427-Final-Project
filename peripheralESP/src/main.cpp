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

#define TOK_ARR_SIZE 10
#include <iostream>
#include <string>
#include <cstring>


using namespace std;
/*
Green flag sticky is peripheral
*/

#define PIR_PIN 13
#define PHOTO_PIN 14
#define DEVICE_NETWORK_PIN 19

volatile bool movementDetected = false;
volatile bool speechDetected = false;
volatile bool deviceDetected = false;
bool prevDetection = false;

long noiseFloor = 0;
int noiseLimit = 5;
int pollFreqPhoto = 5;
int pollTimePhoto = 250;
int photoData;

long start_time;
long end_time;

int64_t timeSinceBoot;

const char* recv_data;
bool receivedData = false;

#define PIR_DATA_ID 1
#define PHOTO_DATA_ID 2
#define DEVICE_NETWORK_DATA_ID 3

// Function that runs if I receive something
void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
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
    string dataToParse = string((char*)data);
    // // Just duplicating what was received and sending it back to controller
    // // Must cast because compiler will not let you copy const data
    // // uint8_t *senderMac_copy = (uint8_t *)mac_addr;
    // // uint8_t *data_copy = (uint8_t *)data;
    // // ESPNow.send_message(senderMac_copy, data_copy, data_len);
	//[<sensor>/system] set [pollRate/sensitivity] <uint> 
	//demand <sensor>

}

// Function that runs if I send something
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("^ Last Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void motionDetectedISR() {
    movementDetected = digitalRead(PIR_PIN);
}

void deviceDetectedISR() {
    // If HIGH, traffic detected
    // If LOW, no traffic detected
}

void sendMessage(int sensorTypeID){
    unsigned long timeSinceBoot = millis();
    int seconds = (timeSinceBoot/1000)%60;
    int minutes = ((timeSinceBoot/1000)/60)%60;
    int hours = ((timeSinceBoot/1000)/60)/60;

    std::string timeSinceBoot_str = "";
    timeSinceBoot_str += std::to_string(hours);
    timeSinceBoot_str += "hr ";
    timeSinceBoot_str += std::to_string(minutes);
    timeSinceBoot_str += "min ";
    timeSinceBoot_str += std::to_string(seconds);
    timeSinceBoot_str += "sec ";

    const char* timeSinceBoot_chars = timeSinceBoot_str.c_str();
    
    String output_str = "";
    const char* output_chars = output_str.c_str();
    
    switch (sensorTypeID) {
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

    ESPNow.send_message(central_mac, (uint8_t*)output_chars, strlen(output_chars));
}

void setup() {
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
    ESPNow.add_peer(central_mac);
    // Register callback functions
    ESPNow.reg_send_cb(onDataSent);
    ESPNow.reg_recv_cb(onRecv);
    // PIR stuff
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionDetectedISR, RISING);
    attachInterrupt(digitalPinToInterrupt(DEVICE_NETWORK_PIN), deviceDetectedISR, CHANGE);
    
    String timeLoggingMsg = "Times shown are relative to system bootup~";
    const char * logMsgConvert = timeLoggingMsg.c_str();
    ESPNow.send_message(central_mac, (uint8_t *)logMsgConvert, strlen(logMsgConvert));
}

void calibratePhoto(){
    long count = 0;
    long reading = 0;

    while(end_time - start_time <= 2000){
        reading += analogRead(PHOTO_PIN);
        count++;
        end_time = millis();
    }
    long noiseFloor = reading / count;
}

void readPhoto(){
    //int largestReading = analogRead(PHOTO_PIN);
    int data = 0;
	cli();
    data = digitalRead(PHOTO_PIN);
	sei();
	sendMessage(data);
}

void readPIR(){
	int data;
	cli();
	data = analogRead(PIR_PIN);
	sei();
	sendMessage(data);
}
volatile int sensorPeriod;
//have a integer holding the ammount of times per second, if its been 1 second divided on 
void loop() {
    // cli();
	
    // Serial.println("in critical section");
    bool PIRDetectVar = movementDetected;
    bool photoVar = speechDetected;
    bool deviceVar = deviceDetected;
    // sei();

	int prev_time_PIR;
	int cur_time_PIR;
	
	//I want this main loop to run, I want ot check the value of sensor period
	//, and if the current time is greater than the last polled time - sensor period

	//if(end_time - start_time < pollTimePhoto/pollFreqPhoto) end_time = millis();

	//readPhoto();
	//largestReading = (curReading > largestReading) ? curReading : largestReading;
    
    if (PIRDetectVar) {
        Serial.println("Movement detected, sending message to controller");
        sendMessage(PIR_DATA_ID);
        cli();
        movementDetected = false;
        sei();
    }
    
    if (photoVar) {
        if (std::abs(photoData-noiseFloor) > std::abs(noiseFloor-noiseLimit)) {
            sendMessage(PHOTO_DATA_ID);
            cli();
            speechDetected = false;
            sei();
        }
    }
    
    if (deviceVar) {
        cli();
        deviceDetected = false;
        sei();
    }

    if (Serial.available()) {
        String data = Serial.readStringUntil('~');
        Serial.print("ESP32: I received your message: ");
        Serial.println(data + "~");
    }



    /*
        For testing receiving data
        Do not delete
    */
    // if (receivedData) {
    //     receivedData = false;
    //     String recv_string = String(recv_data);
    //     recv_string.remove(recv_string.length()-1);
    //     Serial.println(recv_string);
    //     String tokenArray[TOK_ARR_SIZE] = {""};
    //     tokenize(recv_string, tokenArray, TOK_ARR_SIZE);
    //     for (String token : tokenArray) if (!token.isEmpty()) Serial.println(token);
    // }
}