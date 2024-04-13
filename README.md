# Potentiostat

[![PlatformIO CI](https://github.com/ryanchaiyakul/potentiostat/actions/workflows/platformio.yml/badge.svg)](https://github.com/ryanchaiyakul/potentiostat/actions/workflows/platformio.yml)

I2BL potentiostat library and project.

# Goals

1. Create a library to easily program simple potentiostats for various voltammetry methods.
2. Create a collection of tools to develop future potentiostats.
3. Create a simulation suite to visualize and validate design decisions.

# Simulations

Go to the [README.md](CAD/LTSpice/README.md) in CAD/LTSpice folder.

# Tools

Go to the tools folder for a complete collection.

## CPE Models

Generate RC Train models of CPE elements to simulate how the readout circuit responds.

## PWL Voltammetry

Generate PWL files to pass into a voltage source to act as the DAC.

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
- [x] 300+ Hz SWV
- [ ] OTA Configuration
- [ ] EEPROM Persistent Configuration
- [ ] BLE Integration
- [x] Simple "Debug" Mode
- [x] Complete Analytics in "Debug" Mode
- [ ] Unit Testing Suite

# Development Guide

Create a valid class that inherits from Voltammetry defined in [Voltammetry.h](lib/Voltammetry/Voltammetry.h).