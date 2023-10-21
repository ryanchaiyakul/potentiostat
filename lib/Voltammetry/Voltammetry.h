#ifndef VOLTAMMETRY_H
#define VOLTAMMETRY_H
#include <Arduino.h>
#include <Queue.h>

// Constants

#define INTERVALUS 1000 // 1 ms resolution for DAC
#define QUEUEDEPTH 10   // # of actions in buffer

enum UpdateStatus
{
    NONE,
    SHIFTV,
    SAMPLE,
    DONE,
};

/**
 * Data type that corresponds to a specific action
*/
struct Action
{
    unsigned long time;
    UpdateStatus type;
    int payload;
    Action() : time(4294967295), type(UpdateStatus::NONE), payload(0){};                                     // Empty Action blocks till the end of realistic time
    Action(unsigned long time, UpdateStatus type, int payload) : time(time), type(type), payload(payload){}; // General constructor
    bool toExecute(unsigned long queryTime)
    {
        return queryTime >= (this->time);
    };
};

/**
 * Abstract base class
 *
 * Each an object of a subclass represents a specific setting of a Voltammetry method.
 * The constructor should take in the same values as the CHI-660E Potentiostat (internal values can differ)
 *
 * Usage:
 *
 * Call update() as frequently as possible and execute on its return status
 */
class Voltammetry
{
public:
    Voltammetry(int quietMV,
                int relaxMV,
                int minMV,
                int maxMV,
                unsigned long quietTimeUS,
                unsigned long relaxTimeUS);
    virtual void reset();
    int getVoltage();
    UpdateStatus update();
    virtual void printSettings();

protected:
    virtual unsigned long getActiveTime() = 0;
    virtual Action calculateAction() = 0;
    unsigned long getTime();
    void setVoltage(int);

private:
    enum internState
    {
        IDLE,
        QUIET,
        ACTIVE,
        RELAX,
        END,
    };
    unsigned long updateTime;
    unsigned long iniTime;
    unsigned long activeTime;
    unsigned long quietTimeUS;
    unsigned long relaxTimeUS;
    int quietMV;
    int relaxMV;
    int minMV;
    int maxMV;
    int mv;
    internState state;
    Queue<Action> actionQueue = Queue<Action>(QUEUEDEPTH);
    Action nextAction;
    bool isQueue;
};

#endif