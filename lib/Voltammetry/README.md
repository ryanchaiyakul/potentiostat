# Voltammetry

Custom library written for I2BL potentiostats.

# Premise

In the initial design, the microcontroller calculated the voltage for the current time every cycle.

Polling at a high rate and calculating if things should change every iteration, a queue of actions is populated during "Idle" time.

This can possibly be sped up by introducing interrupts where polling can be completely avoided, but this speed already allows for faster SWV with "burst" sampling technique.

# Improvements

- OTA Configuration
- EEPROM Configuration Storage
- Unit Testing Suite