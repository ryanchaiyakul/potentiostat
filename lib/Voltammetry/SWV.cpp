#include <DPV.h>
#include <SWV.h>

SWV::SWV(int startMV,
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
         int maxMV) : Voltammetry(startMV, endMV, minMV, maxMV, quietTimeUS, relaxTimeUS)
{
    this->startMV = startMV - amplitude;
    this->endMV = endMV - amplitude;
    this->incrE = abs(incrE);
    this->amplitude = amplitude;

    this->vertices = vertices;
    for (int i = 0; i < vertices; i++)
    {
        this->vertexMVs[i] = vertexMVs[i] - amplitude;
    }

    this->pulseLengthUS = (1.0 / frequency) * 1000000;
    this->pulseCenterUS = pulseLengthUS / 2;
    this->sampleOffsetUS = sampleOffsetUS;

    this->isForward = (vertices == 0) ? (startMV < endMV ? true : false) : (startMV < vertexMVs[0] ? true : false);
    VtoS = pulseCenterUS - sampleOffsetUS; // Symmetric for SWV
}

void SWV::reset()
{
    Voltammetry::reset();
    if (vertices == 0)
    {
        finishedVertices = true;
        breakMV = endMV;
        isForward = startMV < endMV ? true : false;
    }
    else
    {
        finishedVertices = false;
        breakMV = vertexMVs[0];
        isForward = startMV < vertexMVs[0] ? true : false;
    }
    baseMV = startMV + (isForward ? incrE : -incrE);
    count = 0;
    state = INI;
}

void SWV::printSettings()
{
    Serial.println("SWV Internal Settings:");
    Serial.print("startMV: ");
    Serial.println(startMV);
    Serial.print("endMV: ");
    Serial.println(endMV);
    Serial.print("vertexMVs: [");
    for (int i = 0; i < vertices; i++)
    {
        Serial.print(" ");
        Serial.print(vertexMVs[i]);
        Serial.print(", ");
    }
    Serial.println("]");
    Serial.print("incrE: ");
    Serial.println(incrE);
    Serial.print("amplitude: ");
    Serial.println(amplitude);
    Serial.print("pulseLengthUS: ");
    Serial.println(pulseLengthUS);
    Serial.print("pulseCenterUS: ");
    Serial.println(pulseCenterUS);
    Voltammetry::printSettings();
}

unsigned long SWV::getActiveTime()
{
    unsigned long mvTotal = 0;
    if (vertices > 0)
    {
        mvTotal += abs(vertexMVs[0] - startMV);
        int i = 1;
        for (; i < vertices; i++)
        {
            mvTotal += abs(vertexMVs[i] - vertexMVs[i - 1]);
        }
        mvTotal += abs(endMV - vertexMVs[i - 1]);
    }
    else
    {
        mvTotal = abs(endMV - startMV);
    }
    return (mvTotal / incrE) * pulseLengthUS;
}

Action SWV::calculateAction()
{
    switch (state)
    {
    case INI:
        state = HIGH_S;
        return Action(prevTime, UpdateStatus::SHIFTV, baseMV + amplitude);
    case HIGH_V:
        prevTime += sampleOffsetUS;
        if (prevTime >= getActiveTime())
        {
            return Action();
        }
        baseMV += isForward ? incrE : -incrE;
        if (!finishedVertices)
        {
            // TODO: Think of a faster method of switching directions
            // Determine when to change directions
            if ((isForward && baseMV >= breakMV) || (!isForward && baseMV <= breakMV))
            {
                isForward = !isForward;
                baseMV = breakMV;
                if (++count == vertices)
                {
                    finishedVertices = true;
                }
                else
                {
                    breakMV = vertexMVs[count];
                }
            }
        }
        state = HIGH_S;
        return Action(prevTime, UpdateStatus::SHIFTV, baseMV + amplitude);
    case HIGH_S:
        prevTime += VtoS;
        state = LOW_V;
        return Action(prevTime, UpdateStatus::SAMPLE, 0);
    case LOW_V:
        prevTime += sampleOffsetUS;
        state = LOW_S;
        return Action(prevTime, UpdateStatus::SHIFTV, baseMV - amplitude);
    case LOW_S:
        prevTime += VtoS;
        state = HIGH_V;
        return Action(prevTime, UpdateStatus::SAMPLE, 0);
    }
    return Action();
}