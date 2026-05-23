#include <WString.h>
void tokenize(String str, String arr[], int size);

/*
    Discover built-in/default mac address by running:
        Serial.begin(####);
        WiFi.mode(WIFI_MODE_STA);
        Serial.println(WiFi.macAddress());

    You can also make your own mac address, but you must use set_mac function
*/

extern uint8_t central_mac[];
extern uint8_t peripheral_mac[];

#define PROJ_WIFI_CHANNEL 11