#include <Arduino.h>
#include "Voltammetry.h"

Voltammetry::Voltammetry(int quietMV,
                         int relaxMV,
                         int minMV,
                         int maxMV,
                         unsigned long quietTimeUS,
                         unsigned long relaxTimeUS)
{
    this->quietTimeUS = quietTimeUS;
    this->relaxTimeUS = relaxTimeUS;
    this->quietMV = quietMV;
    this->relaxMV = relaxMV;
    this->minMV = minMV;
    this->maxMV = maxMV;
}

/**
 * Reset object to start of experiment
 */
void Voltammetry::reset()
{
    activeTime = getActiveTime();
    state = internState::IDLE;
    iniTime = micros();
    prevTime = 0;
    mv = 0;
    actionQueue.clear();
}

int Voltammetry::getVoltage()
{
    return mv;
}

UpdateStatus Voltammetry::update()
{
    updateTime = micros() - iniTime;
    switch (state)
    {
    case IDLE:
        // IDLE immediately transitions to quiet on first call
        nextAction = calculateAction();
        actionQueue.push(calculateAction());   
        setVoltage(quietMV);
        state = internState::QUIET;
        iniTime = micros(); // Update iniTime in case reset() and first update is far apart
        return UpdateStatus::SHIFTV;
    case QUIET:
        // Transition to Active after quietTime ends
        if (updateTime >= quietTimeUS)
        {
            state = internState::ACTIVE;
            iniTime += quietTimeUS;
        }
        break;
    case ACTIVE:
        // Execute action if necessary
        if (nextAction.toExecute(updateTime))
        {
            UpdateStatus ret = nextAction.type;
            if (ret == SHIFTV)
            {
                setVoltage(nextAction.payload);
            }
            nextAction = actionQueue.pop();
            return ret;
        }
        // Exit status
        if (updateTime >= activeTime)
        {
            setVoltage(relaxMV);
            state = internState::RELAX;
            iniTime += activeTime;
            return UpdateStatus::SHIFTV;
        }
        // Add to stack if not full
        if (actionQueue.count() < QUEUEDEPTH)
        {
            actionQueue.push(calculateAction());
        }
        break;
    case RELAX:
        if (updateTime >= relaxTimeUS)
        {
            state = internState::END;
        }
        break;
    case END:
        return UpdateStatus::DONE;
    }
    return UpdateStatus::NONE;
}

unsigned long Voltammetry::getTime()
{
    return updateTime;
}

/**
 * Sets internal stored voltage to clamped mv value
 */
void Voltammetry::setVoltage(int mv)
{
    this->mv = (mv < minMV) ? minMV : (mv > maxMV) ? maxMV
                                                   : mv;
}

/**
 * Serial prints Voltammtery status
 */
void Voltammetry::printSettings()
{
    Serial.print("quietTimeUS: ");
    Serial.println(quietTimeUS);
    Serial.print("restTimeUS: ");
    Serial.println(relaxTimeUS);
}