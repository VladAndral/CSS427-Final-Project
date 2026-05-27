#include "utils.h"

uint8_t controller_mac[] = {0xA0, 0xB7, 0x65, 0x1A, 0x7C, 0x30};
uint8_t peripheral_mac[] = {0xA0, 0xB7, 0x65, 0x22, 0xFF, 0x94};

String targetNames[] = { 
    "pir",
    "photo",
    "rf",
    "system"
};

String actionNames[] = {
    "demand",
    "set",
    "get",
    "schedule"
};

String AttributeNames_sensor[] {
    "pollrate",
    "sensitivity"
};

String AttributeNames_system[] {
    "mode"
};

String AttributeValues_system[] {
    "normal",
    "maintenance",
    "quiet",
    "lockdown"
};

void tokenize(String str, String arr[], int size) {
    String token = "";
    int arrPos = 0;
    
    for (char curChar : str) {
        if (curChar != ' ') {
            token += curChar;
        } else {
            token.toLowerCase();
            arr[arrPos++] = token;
            
            token = ""; 
        }
    }
    
    // Catch the final token at the end of the string
    arr[arrPos] = token;
}