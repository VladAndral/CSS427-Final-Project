// Photo.cpp
#include "Photo.h"

Photo::Photo(int pin, volatile bool* intrFlag) : SensorBase(pin, intrFlag) 
{
    sensitivityAdjustable = true;
}

float Photo::getReading()
{
    return (float)analogRead(pin_num);
}

bool Photo::calibrate()
{
    long count = 0;
    long reading = 0;
    
    // Reset timers at the top of the function to prevent time-leaks
    unsigned long start_time = millis();
    unsigned long end_time = start_time;

    while (end_time - start_time <= 2000)
    {
        reading += analogRead(pin_num);
        count++;
        end_time = millis();
    }
    
    noiseFloor = reading / count;
    trigMin = noiseFloor * 1.2;
    trigMax = noiseFloor * 10;
    sensitivityStep = (trigMax - trigMin) / 100;
    
    return true;
}