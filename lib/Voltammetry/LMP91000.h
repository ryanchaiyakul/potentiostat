#ifndef LMP91000_H
#define LMP91000_H
#include <Arduino.h>

#define LMP91000_OPAMPMV 3300
#define LMP91000_MINMV 1520

const double TIA_BIAS[14] = {0, 0.01, 0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.14, 0.16, 0.18, 0.20, 0.22, 0.24};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
const uint16_t MAX_MV[12] = {
    LMP91000_OPAMPMV * TIA_BIAS[2],
    LMP91000_OPAMPMV *TIA_BIAS[3],
    LMP91000_OPAMPMV *TIA_BIAS[4],
    LMP91000_OPAMPMV *TIA_BIAS[5],
    LMP91000_OPAMPMV *TIA_BIAS[6],
    LMP91000_OPAMPMV *TIA_BIAS[7],
    LMP91000_OPAMPMV *TIA_BIAS[8],
    LMP91000_OPAMPMV *TIA_BIAS[9],
    LMP91000_OPAMPMV *TIA_BIAS[10],
    LMP91000_OPAMPMV *TIA_BIAS[11],
    LMP91000_OPAMPMV *TIA_BIAS[12],
    LMP91000_OPAMPMV *TIA_BIAS[13],
};
#pragma GCC diagnostic pop

struct VBiasPair
{
    uint16_t mV;
    uint8_t bias;

    VBiasPair() : mV(0), bias(0){};
    VBiasPair(uint16_t mV, uint8_t bias) : mV(mV), bias(bias){};
};

class LMP91000
{
public:
    LMP91000();
    VBiasPair calculateVBiasPair(uint16_t);
    uint16_t calculateV(uint16_t, uint8_t);
private:
    uint8_t calculateBias(uint16_t);
};
#endif