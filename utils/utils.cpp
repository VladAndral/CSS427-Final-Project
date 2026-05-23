#include "utils.h"
// #include "/Users/vladimir/Library/Arduino15/packages/arduino/hardware/avr/1.8.7/cores/arduino/WString.h"

uint8_t central_mac[] = {0xA0, 0xB7, 0x65, 0x1A, 0x7C, 0x30};
uint8_t peripheral_mac[] = {0xA0, 0xB7, 0x65, 0x22, 0xFF, 0x94};

void tokenize(String str, String arr[], int size) {
    String token = "";
    int arrPos = 0;
    
    for (char curChar : str) {
        if (curChar != ' ') {
            token += curChar;
        } else {
            arr[arrPos++] = token;
            token = ""; 
        }
    }
    
    // Catch the final token at the end of the string b/c there is no space at end
    arr[arrPos] = token;
}