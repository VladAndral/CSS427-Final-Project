#include "utils.h"
// #include "/Users/vladimir/Library/Arduino15/packages/arduino/hardware/avr/1.8.7/cores/arduino/WString.h"

String* tokenize(String& str) {
    String toReturn[10];
    String token = "";

    int toReturnPos = 0;
    for (char curChar : str) {
        while (curChar != ' ') token += curChar;
        toReturn[toReturnPos] = token;
    }

    return toReturn;
}