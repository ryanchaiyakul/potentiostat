#ifndef DPV_H
#define DPV_H
#include <Arduino.h>
#include <Voltammetry.h>

class DPV : public Voltammetry
{
public:
    DPV(int startMV,
        int endMV,
        int incrE,
        int amplitude,
        unsigned long pulseWidthUS,
        unsigned long pulseLengthUS,
        unsigned long sampleOffsetUS,
        unsigned long quietTimeUS,
        unsigned long relaxTimeUS,
        int minMV,
        int maxMV);
    virtual void reset();
    virtual void printSettings();

protected:
    virtual unsigned long getActiveTime();
    virtual Action calculateAction();

private:
    int startMV;
    int endMV;
    int incrE;
    int amplitude;
    unsigned long pulseWidthUS;
    unsigned long pulseLengthUS;
    unsigned long sampleOffsetUS;
    int baseMV;
    enum dpvState
    {
        INI,
        LOW_V,
        LOW_S,
        HIGH_V,
        HIGH_S
    } state;
    unsigned long prevTime;
    unsigned long lowVToLowS, highVtoHighS;
};

#endif