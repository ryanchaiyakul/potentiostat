#ifndef SWV_H
#define SWV_H
#include <Arduino.h>
#include <Voltammetry.h>

class SWV : public Voltammetry
{
public:
    SWV(int startMV,
        int endMV,
        int vertexMVs[4],
        int vertices,
        int incrE,
        int amplitude,
        unsigned frequency,
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
    int vertexMVs[4];
    int vertices;
    int incrE;
    int amplitude;
    unsigned long pulseLengthUS;
    unsigned long pulseCenterUS;
    unsigned long sampleOffsetUS;
    bool isForward;
    int baseMV;
    bool finishedVertices;
    int breakMV;
    uint8_t count;
    enum dpvState
    {
        INI,
        LOW_V,
        LOW_S,
        HIGH_V,
        HIGH_S
    } state;
    unsigned long prevTime;
    unsigned long VtoS;
};

#endif