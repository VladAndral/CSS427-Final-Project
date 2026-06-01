#include "SensorSystem.h"

SensorSystem::SensorSystem(PIR pir, Photo photo, RF rf) : pir(pir), photo(photo), rf(rf) {}

SensorSystem::~SensorSystem()
{
}

void SensorSystem::setMode(Attr_Val)
{
}

Attr_Val SensorSystem::getMode()
{
    return Attr_Val();
}

void SensorSystem::setSensitivity(int sensitivity)
{
}

String SensorSystem::getSensitivity()
{
    return String();
}

void SensorSystem::setPollRate(int pollRate)
{
}

String SensorSystem::getPollRate()
{
    return String();
}
