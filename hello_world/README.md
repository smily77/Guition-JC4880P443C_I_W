# Guition JC4880P443C_I_W - Hello World Example

A simple LVGL Hello World example for the Guition JC4880P443C_I_W display board powered by the ESP32-P4 chip.

This is a stripped down version of the example code that was provided by the board manufacturer ready to be used for your own project.

## Hardware

- **Display Board**: Guition JC4880P443C_I_W
- **MCU**: ESP32-P4
- **LCD Controller**: ST7701
- **Touch Controller**: GT911
- **Display Resolution**: 480x800 pixels
- **Touch Interface**: I2C (SDA: GPIO7, SCL: GPIO8)

## Features

- ST7701 LCD driver with DPI interface
- GT911 capacitive touch controller driver
- LVGL v8 graphics library integration
- Double buffering using SPIRAM for smooth rendering
- Display rotation support with automatic touch coordinate transformation
- Simple Hello World label demonstration

## Project Structure

```
.
├── hello_world.ino          # Main Arduino sketch
├── pins_config.h            # Pin definitions and display configuration
├── lv_conf.h                # LVGL configuration
└── src/
    ├── lcd/
    │   ├── st7701_lcd.h     # ST7701 LCD driver header
    │   ├── st7701_lcd.cpp   # ST7701 LCD driver implementation
    │   ├── esp_lcd_st7701.h # ESP-IDF LCD interface
    │   └── esp_lcd_st7701_interface.h
    └── touch/
        ├── gt911_touch.h    # GT911 touch driver header
        ├── gt911_touch.cpp  # GT911 touch driver implementation
        ├── esp_lcd_touch.h  # ESP-IDF touch interface
        └── esp_lcd_touch_gt911.h
```

## Pin Configuration

The pin configuration is defined in [pins_config.h](pins_config.h):

| Peripheral | Pin | GPIO |
| ---------- | --- | ---- |
| Touch SDA  | I2C | 7    |
| Touch SCL  | I2C | 8    |
| Touch RST  | N/A | -1   |
| Touch INT  | N/A | -1   |
| LCD RST    | N/A | -1   |
| LCD LED    | N/A | -1   |

## Requirements

### Hardware

- Guition JC4880P443C_I_W display board with ESP32-P4

### Software

- Arduino IDE or PlatformIO
- ESP32-P4 board support package
- LVGL library (v8.x)
- ESP-IDF components for LCD and I2C

## Building and Uploading

1. Install the ESP32-P4 board support in Arduino IDE
2. Install the LVGL library
3. Open [hello_world.ino](hello_world.ino) in Arduino IDE
4. Select the appropriate ESP32-P4 board from Tools > Board
5. Select the correct COM port
6. Click Upload

## Code Overview

The main sketch ([hello_world.ino](hello_world.ino)) performs the following:

1. **Initialization** (in `setup()`):

   - Configures I2C for the GT911 touch controller
   - Initializes the ST7701 LCD driver
   - Initializes the GT911 touch driver
   - Sets up LVGL with double buffering in SPIRAM
   - Registers display and touch input drivers
   - Creates a simple centered "Hello World" label

2. **Main Loop** (in `loop()`):
   - Calls `lv_timer_handler()` to process LVGL tasks
   - 5ms delay between iterations

### Key Features

- **Double Buffering**: Uses SPIRAM for two full-screen buffers to ensure smooth rendering
- **Touch Rotation**: Automatically updates touch coordinates when display rotation changes
- **DPI Interface**: Uses ESP32-P4's DPI peripheral for efficient LCD communication
- **Optimized Build**: Compiled with `-O3` optimization flag

## Display Driver (ST7701)

The ST7701 LCD driver is implemented in [src/lcd/st7701_lcd.cpp](src/lcd/st7701_lcd.cpp). It provides:

- RGB565 color format support
- DPI panel interface configuration
- Hardware-accelerated bitmap drawing

## Touch Driver (GT911)

The GT911 touch driver is implemented in [src/touch/gt911_touch.cpp](src/touch/gt911_touch.cpp). It provides:

- I2C communication with the touch controller
- Multi-touch coordinate reading
- Rotation transformation support

## Customization

To modify the Hello World example:

1. Edit the UI code in `setup()` function ([hello_world.ino:145-149](hello_world.ino#L145-L149))
2. Use LVGL's widget creation functions to build your interface
3. Refer to the [LVGL documentation](https://docs.lvgl.io/) for available widgets and styling options
