#include <Arduino.h>
#include <SPI.h>
#include <ADS131A04.h>

// TODO: Consider SPI
ADS131A04::ADS131A04(uint8_t cs)
{
    this->cs = cs;
}

/**
 * Call CMD and returns its acknowledgement
 *
 * Higher call than writeCMD
 */
uint16_t ADS131A04::callCMD(uint16_t cmd)
{
    writeCMD(cmd);
    return writeCMD(ADS131A04_CMD_NULL);    // Response is from previous CMD
}

/**
 * Send a system command and returns previous acknowledgement
 */
uint16_t ADS131A04::writeCMD(uint16_t cmd)
{
    SPI.beginTransaction(SPISettings(ADS131A04_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(cs, LOW);
    uint16_t bfr = SPI.transfer16(cmd);
    digitalWrite(cs, HIGH);
    // TODO: Figure out if all remaining bits have to cycle
    SPI.endTransaction();
    return bfr;
}

/**
 * Read a single 16 bit register
 */
uint16_t ADS131A04::readReg(uint16_t addr)
{
    return callCMD(ADS131A04_CMD_RREG | (addr << 8));
}

/**
 * Write to a single 16 bit register
 */
uint16_t ADS131A04::writeReg(uint16_t addr, uint16_t data)
{
    return callCMD(ADS131A04_CMD_WREG | (addr << 8) | data);
}

/**
 * Enable all channels
 */
void ADS131A04::enable()
{
    writeReg(ADS131A04_ADC_ENA, 0b1111); // pg. 69
}

/**
 * Disable all channels
 */
void ADS131A04::enable()
{
    writeReg(ADS131A04_ADC_ENA, 0b0000); // pg. 69
}

void ADS131A04::setGain(uint8_t channel, uint16_t gain)
{
    uint16_t addr;
    switch (channel)
    {
    case 1:
        addr = ADS131A04_ADC1;
        break;
    case 2:
        addr = ADS131A04_ADC2;
        break;
    case 3:
        addr = ADS131A04_ADC3;
        break;
    case 4:
        addr = ADS131A04_ADC4;
        break;
        // Add default errror case
    }
    writeReg(addr, gain);
}

/**
 * Read all four channels and return the data in a uint16_t[4]
*/
uint16_t* ADS131A04::readChannels() {
    uint16_t ret[4];
    SPI.beginTransaction(SPISettings(ADS131A04_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(cs, LOW);
    SPI.transfer16(ADS131A04_CMD_NULL); // Convention
    // Four channel data follows the status response (pg. 44)
    ret[0] = SPI.transfer16(ADS131A04_CMD_NULL);
    ret[1] = SPI.transfer16(ADS131A04_CMD_NULL);
    ret[2] = SPI.transfer16(ADS131A04_CMD_NULL);
    ret[3] = SPI.transfer16(ADS131A04_CMD_NULL);
    digitalWrite(cs, HIGH);
    SPI.endTransaction();
    return ret;
}