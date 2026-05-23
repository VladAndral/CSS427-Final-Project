/*
 * "THE BEER-WARE LICENSE" (Revision 42):
 * regenbogencode@gmail.com wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return
 */
#include <utils.h>
#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif ESP32
#include <WiFi.h>
#endif
#include "ESPNowW.h"
#include <esp_wifi.h>

/*
            Red flag sticky is central
*/

// Callback function
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("^ Last Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void onRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    // if it could be a string, print as one
    if (data[data_len - 1] == '~') {
        // Serial.printf("%s\n", data);
        for (int i = 0; i < data_len-1; i++) {
            Serial.printf("%c", data[i]);
        }
        Serial.println("");
    }
    // // additionally print as hex
    // Serial.print("Data in hex: ");
    // for (int i = 0; i < data_len; i++) Serial.printf("0x%x ", data[i]);
    Serial.println("");
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
    esp_wifi_set_channel(PROJ_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    ESPNow.init();

    // If you created a custom mac address, must use this function
    ESPNow.set_mac(central_mac);
    ESPNow.add_peer(peripheral_mac);
    // Must add peer to send data back to it
    ESPNow.reg_send_cb(onDataSent);
    // Register callback functions
    ESPNow.reg_recv_cb(onRecv);
}

void loop() {
    // static uint8_t a = 0;
    // delay(1000);
    // ESPNow.send_message(peripheral_mac, &a, 1);
    // // ++ operation increments the var after being used
    // Serial.println(a++);

    if (Serial.available()) {
        Serial.print("Received user input: ");
        String userInput = Serial.readStringUntil('\n');
        Serial.println(userInput);

        // String tokenArray[TOK_ARR_SIZE] = {""};

        // tokenize(userInput, tokenArray);

        // for (String token : tokenArray) Serial.println(token);
        // userInput += '~';
        userInput.setCharAt(userInput.length()-1, '~');
        const char* userInput_cstr = userInput.c_str();
        int exitVal = ESPNow.send_message(peripheral_mac, (uint8_t*)userInput_cstr, userInput.length());
        Serial.println(exitVal);

    }


}

/*
    [<sensor>/system] set [pollRate/sensitivity] <uint>
    demand <sensor>

*/