/*
 * "THE BEER-WARE LICENSE" (Revision 42):
 * regenbogencode@gmail.com wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return
 */
#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif ESP32
#include <WiFi.h>
#endif
#include "ESPNowW.h"

// Red flag sticky is central

// Must be the receiver's actual mac address; you can't just make one up here
uint8_t my_mac[] = {0xA0, 0xB7, 0x65, 0x1A, 0x7C, 0x30};
uint8_t receiver_mac[] = {0xA0, 0xB7, 0x65, 0x22, 0xFF, 0x94};

// Callback function
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("^ Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    Serial.println("Got what peri sent");
}

void setup() {
    Serial.begin(9600);
    Serial.println("ESPNow sender Demo");
#ifdef ESP8266
    WiFi.mode(WIFI_STA); // MUST NOT BE WIFI_MODE_NULL
#elif ESP32
    WiFi.mode(WIFI_MODE_STA);
#endif
    WiFi.disconnect();
    ESPNow.init();
    ESPNow.add_peer(receiver_mac);
    ESPNow.reg_send_cb(onDataSent);
    ESPNow.reg_recv_cb(onRecv);
}

void loop() {
    static uint8_t a = 0;
    delay(1000);
    ESPNow.send_message(receiver_mac, &a, 1);
    Serial.println(a++);
}