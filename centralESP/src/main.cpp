/*
 * "THE BEER-WARE LICENSE" (Revision 42):
 * regenbogencode@gmail.com wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return
 */
#include "../../utils.h"
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

/*
    Discover built-in/default mac address by running:
        Serial.begin(####);
        WiFi.mode(WIFI_MODE_STA);
        Serial.println(WiFi.macAddress());

    You can also make your own mac address, but you must use set_mac function
*/
uint8_t my_mac[] = {0xA0, 0xB7, 0x65, 0x1A, 0x7C, 0x30};
// Must be the receiver's actual mac address; you can't just make one up here
uint8_t peripheral_mac[] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};

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
    esp_wifi_set_channel(12, WIFI_SECOND_CHAN_NONE);
    ESPNow.init();

    // If you created a custom mac address, must use this function
    ESPNow.set_mac(my_mac);
    ESPNow.add_peer(peripheral_mac);
    // Must add peer to send data back to it
    ESPNow.reg_send_cb(onDataSent);
    // Register callback functions
    ESPNow.reg_recv_cb(onRecv);
}

String[] tokenize(String& str) {
    String toReturn[];
    String token = "";
    for (char curChar : str) {
        while (curChar != ' ') token += curChar;

    }
}

void loop() {
    // static uint8_t a = 0;
    // delay(1000);
    // ESPNow.send_message(peripheral_mac, &a, 1);
    // ++ operation increments the var after being used
    // Serial.println(a++);

    if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');

        String* tokenize(&userInput);
    }


}

/*
    [<sensor>/system] set [pollRate/sensitivity] <uint>
    demand <sensor>

*/