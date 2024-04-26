#include <LMP91000.h>

LMP91000::LMP91000() {

}

/**
 * Determines the optimal Voltage and Bias pairing using binary sort
*/
VBiasPair LMP91000::calculateVBiasPair(uint16_t voltage) {
    uint8_t bias = calculateBias(voltage);
    return VBiasPair(calculateV(voltage, bias), bias);
}

/**
 * Determines the optimal bias for a specified voltage
*/
uint8_t LMP91000::calculateBias(uint16_t voltage) {
    int i;
    int l = 0;
    int h = 11;
    while (l <= h) {
        i = (l + h) / 2;
        if (MAX_MV[i] <= voltage && (i == h || MAX_MV[i+1] > voltage)) {
            return i;
        } else if (MAX_MV[i] <= voltage) {
            l = i + 1;
        } else {
            h = i - 1;
        }
    }
    return 0;
}

/**
 * Returns the vref voltage to result in the requested voltage with requested bias setting
*/
uint16_t LMP91000::calculateV(uint16_t voltage, uint8_t bias) {
    return ((double) voltage) / TIA_BIAS[bias];
}