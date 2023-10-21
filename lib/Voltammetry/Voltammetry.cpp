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
    isQueue = false;
}

/**
 * Reset object to start of experiment
 */
void Voltammetry::reset()
{
    state = internState::IDLE;
    activeTime = getActiveTime();
    iniTime = micros();
    actionQueue.clear();
    mv = 0;
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
        setVoltage(quietMV);
        state = internState::QUIET;
        iniTime = micros();
        return UpdateStatus::SHIFTV;
    case QUIET:
        if (updateTime >= quietTimeUS)
        {

            nextAction = calculateAction();
            actionQueue.push(calculateAction()); // need to put one in queue to prevent empty action
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