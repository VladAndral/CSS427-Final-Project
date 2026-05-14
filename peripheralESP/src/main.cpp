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

// Green flag sticky is peripheral

/*
    Discovered by running:
        Serial.begin(####);
        WiFi.mode(WIFI_MODE_STA);
        Serial.println(WiFi.macAddress());
*/
uint8_t my_mac[] = {0xA0, 0xB7, 0x65, 0x22, 0xFF, 0x94};
uint8_t controller_mac[] = {0xA0, 0xB7, 0x65, 0x1A, 0x7C, 0x30};

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
    for (int i = 0; i < data_len; i++) {
        Serial.printf("(hex) %x", data[i]);
    }
    Serial.println("");

    uint8_t *senderMac_copy = (uint8_t *)mac_addr;
    uint8_t *data_copy = (uint8_t *)data;
    ESPNow.send_message(senderMac_copy, data_copy, data_len);   
}

// Callback function
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("^ Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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
    // You can create a custom mac address and use this function to set it
    // ESPNow.set_mac(my_mac);
    ESPNow.init();
    ESPNow.add_peer(controller_mac);
    ESPNow.reg_send_cb(onDataSent);
    ESPNow.reg_recv_cb(onRecv);
}

void loop() {}