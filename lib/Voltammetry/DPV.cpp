#include <Arduino.h>
#include <DPV.h>

DPV::DPV(int startMV,
         int endMV,
         int incrE,
         int amplitude,
         unsigned long pulseWidthUS,
         unsigned long pulseLengthUS,
         unsigned long sampleOffsetUS,
         unsigned long quietTimeUS,
         unsigned long relaxTimeUS,
         int minMV,
         int maxMV) : Voltammetry(startMV, endMV, minMV, maxMV, quietTimeUS, relaxTimeUS)
{
    this->startMV = startMV - amplitude;
    this->endMV = endMV - amplitude;
    this->incrE = startMV < endMV ? abs(incrE) : -abs(incrE);
    this->amplitude = amplitude;

    this->pulseWidthUS = pulseWidthUS;
    this->pulseLengthUS = pulseLengthUS;
    this->sampleOffsetUS = sampleOffsetUS;

    lowVToLowS = (pulseLengthUS - pulseWidthUS) - sampleOffsetUS;
    highVtoHighS = pulseWidthUS - sampleOffsetUS;
}

void DPV::reset()
{
    Voltammetry::reset();
    baseMV = startMV + incrE;
    prevTime = 0; // initial sets to 0 instead of incrementing
    state = INI;
}

void DPV::printSettings()
{
    Serial.println("DPV Internal Settings:");
    Serial.print("startMV: ");
    Serial.println(startMV);
    Serial.print("endMV: ");
    Serial.println(endMV);
    Serial.print("incrE: ");
    Serial.println(incrE);
    Serial.print("amplitude: ");
    Serial.println(amplitude);
    Serial.print("pulseStartUS: ");
    Serial.println(pulseWidthUS);
    Serial.print("pulseLengthUS: ");
    Serial.println(pulseLengthUS);
    Voltammetry::printSettings();
}

unsigned long DPV::getActiveTime()
{
    return ((endMV - startMV) / incrE) * pulseLengthUS;
}

Action DPV::calculateAction()
{
    switch (state)
    {
    case INI:
        state = LOW_S;
        return Action(prevTime, UpdateStatus::SHIFTV, baseMV);
    case LOW_V:
        prevTime += sampleOffsetUS;
        if (prevTime >= getActiveTime())
        {
            return Action();
        }
        baseMV += incrE;
        state = LOW_S;
        return Action(prevTime, UpdateStatus::SHIFTV, baseMV);
    case LOW_S:
        prevTime += lowVToLowS; // (pulseLengthUS - pulseWidthUS) - sampleOffsetUS
        state = HIGH_V;
        return Action(prevTime, UpdateStatus::SAMPLE, 0);
    case HIGH_V:
        prevTime += sampleOffsetUS;
        state = HIGH_S;
        return Action(prevTime, UpdateStatus::SHIFTV, baseMV + amplitude);
    case HIGH_S:
        prevTime += highVtoHighS; // pulseWidthUS - sampleOffsetUS
        state = LOW_V;
        return Action(prevTime, UpdateStatus::SAMPLE, 0);
    }
    return Action();
}