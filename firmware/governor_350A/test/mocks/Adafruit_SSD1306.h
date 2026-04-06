#pragma once
#include "Wire.h"
#include <stdint.h>

#define SSD1306_WHITE        1
#define SSD1306_BLACK        0
#define SSD1306_SWITCHCAPVCC 0x2

class Adafruit_SSD1306 {
public:
    Adafruit_SSD1306(uint8_t w, uint8_t h, TwoWire* wire, int8_t rst = -1) {}
    bool   begin(uint8_t vcs = SSD1306_SWITCHCAPVCC, uint8_t addr = 0x3C) { return true; }
    void   clearDisplay() {}
    void   setTextSize(uint8_t) {}
    void   setTextColor(uint16_t) {}
    void   setCursor(int16_t, int16_t) {}
    void   drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
    void   drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
    void   fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
    void   display() {}
    template<typename T> size_t print(T)   { return 0; }
    template<typename T> size_t println(T) { return 0; }
    size_t println()                       { return 0; }
};
