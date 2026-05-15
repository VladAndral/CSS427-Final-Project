/*
* "THE BEER-WARE LICENSE" (Revision 42):
* regenbogencode@gmail.com wrote this file. As long as you retain this notice
* you can do whatever you want with this stuff. If we meet some day, and you
* think this stuff is worth it, you can buy me a beer in return
*/
#include <Arduino.h>
#include <esp_wifi.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif ESP32
#include <WiFi.h>
#endif
#include "ESPNowW.h"

const int PIR_pin = GPIO_NUM_13;

volatile bool intruderDetected = false;
bool prevDetection = false;

int64_t timeSinceBoot;

/*
Green flag sticky is peripheral
*/

// See controller for setting custom mac address
uint8_t my_mac[] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};

// Must be controller's actual operating mac address
uint8_t controller_mac[] = {0xA0, 0xB7, 0x65, 0x1A, 0x7C, 0x30};

// Function that runs if I receive something
void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
    mac_addr[5]);
    Serial.print("Last Packet Recv from: ");
    Serial.println(macStr);
    Serial.print("Last Packet Recv Data: ");
    // if it could be a string, print as one
    if (data[data_len - 1] == 0)
    Serial.printf("%s\n", data);
    // additionally print as hex
    for (int i = 0; i < data_len; i++)
    {
        Serial.printf("(hex) %x", data[i]);
    }
    Serial.println("");
    
    // Just duplicating what was received and sending it back to controller
    // Must cast because compiler will not let you copy const data
    uint8_t *senderMac_copy = (uint8_t *)mac_addr;
    uint8_t *data_copy = (uint8_t *)data;
    ESPNow.send_message(senderMac_copy, data_copy, data_len);
}

// Function that runs if I send something
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("^ Last Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void readPIR() {
    intruderDetected = digitalRead(PIR_pin);
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
    esp_wifi_set_channel(12, WIFI_SECOND_CHAN_NONE);
    ESPNow.init();
    // Must add peer to send data back to it
    ESPNow.set_mac(my_mac);
    ESPNow.add_peer(controller_mac);
    // Register callback functions
    ESPNow.reg_send_cb(onDataSent);
    ESPNow.reg_recv_cb(onRecv);
    // PIR stuff
    pinMode(PIR_pin, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIR_pin), readPIR, CHANGE);

    String timeLoggingMsg = "Times shown are relative to system bootup";
    const char * logMsgConvert = timeLoggingMsg.c_str();
    ESPNow.send_message(controller_mac, (uint8_t *)logMsgConvert, strlen(logMsgConvert));
}

void loop() {
    cli();
    bool PIRDetectVar = intruderDetected;
    sei();
    if (PIRDetectVar) {
        
        int64_t timeSinceBoot = millis();
        char buffer[64];
        itoa(timeSinceBoot, buffer, 10);
        const char* timeSinceBoot_str = buffer;
        String PIR_sense_time_str = "";

        PIR_sense_time_str += "PIR Sensor: Movement detected at ";
        PIR_sense_time_str += timeSinceBoot_str;
        PIR_sense_time_str += "time units change later";
        PIR_sense_time_str += "~";

        const char * charConvert = PIR_sense_time_str.c_str();

        Serial.println("Sending PIR sensor info to controller");
        // NOTE: strlen does NOT count the null terminator (ASCII 0)
        ESPNow.send_message(controller_mac, (uint8_t*)charConvert, strlen(charConvert));
        // Serial.println("\r\n    \r\n");
        // Serial.println(charSentOut);
        cli();
        intruderDetected = false;
        sei();
    }
}