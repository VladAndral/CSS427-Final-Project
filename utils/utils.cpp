#include "utils.h"
// #include "/Users/vladimir/Library/Arduino15/packages/arduino/hardware/avr/1.8.7/cores/arduino/WString.h"

void tokenize(String str, String arr[], int size) {
    String token = "";
    int arrPos = 0;
    
    for (char curChar : str) {
        while (curChar != ' ') token += curChar;
        arr[arrPos] = token;
    }
}