// Pin mapping for the ESP32-C3 Super Mini.
#pragma once

// WS2812B data line. GPIO10 is not a strapping pin and not used by USB/JTAG.
// Put a ~330 ohm series resistor close to the first LED.
constexpr int PIN_LED_DATA = 10;

// BOOT button (GPIO9, strapping pin, active-low). Usable as an input once
// the firmware is running.
constexpr int PIN_BUTTON_BOOT = 9;

// Onboard blue LED (GPIO8, active-low: LOW = on).
constexpr int PIN_ONBOARD_LED = 8;
