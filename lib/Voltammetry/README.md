# Voltammetry

Custom library written for I2BL potentiostats.

# Premise

Instead of polling at a high rate and calculating if things should change every iteration, a queue of actions to undertake is populated during "Idle" time and only the time of the immediate action is polled.

This can possibly be sped up by introducing interrupts where polling can be completely avoided, but this speed already allows for faster SWV with "burst" sampling technique.

# Improvements

- OTA Configuration
- EEPROM Configuration Storage
- Unit Testing Suite