# Potentiostat

I2BL potentiostat library and project.

# Goals

Create a library to easily program simple potentiostats for various voltammetry methods.

# Usage

```bash
# submodules are used in this repository for third party libraries
git clone --recurse-submodules git@github.com:ryanchaiyakul/potentiostat.git
```

1. Open the project in [VS Code](https://code.visualstudio.com/) with the [PlatformIO extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed.
2. Connect the Potentiostat to your computer by USB.
3. Navigate to the PlatformIO panel and click "Upload" or use the Command Palette (CNTL + SHIFT + P) and select "Upload".
4. Use the Serial Monitor in the terminal bar and select the baudrate of 230400.

# Features

- [x] Differential Pulse Voltammetry
- [x] Square Wave Voltammetry
- [ ] Cyclic Voltammetry
- [ ] 300+ Hz SWV (?)
- [ ] OTA Configuration
- [ ] EEPROM Persistent Configuration
- [ ] BLE Integration
- [x] Simple "Debug" Mode
- [ ] Complete Analytics in "Debug" Mode
- [ ] Unit Testing Suite

# Development Guide

Create a valid class that inherits from Voltammetry defined in [Voltammetry.h](lib/Voltammetry/Voltammetry.h).