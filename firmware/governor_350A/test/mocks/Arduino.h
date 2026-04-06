#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <string>

// ── Constants ─────────────────────────────────────────────────────────────
#define HIGH         1
#define LOW          0
#define INPUT_PULLUP 0x02
#define OUTPUT       0x01
#define FALLING      2
#define RISING       3

// ── Mock time ─────────────────────────────────────────────────────────────
uint32_t mock_millis = 0;
uint32_t mock_micros = 0;
inline uint32_t millis() { return mock_millis; }
inline uint32_t micros() { return mock_micros; }
inline void delay(uint32_t) {}

// ── Mock GPIO ─────────────────────────────────────────────────────────────
uint8_t mock_pin[20] = {};
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t val) { if (pin < 20) mock_pin[pin] = val; }
inline int  digitalRead (uint8_t pin)              { return (pin < 20) ? mock_pin[pin] : LOW; }

// ── Math ──────────────────────────────────────────────────────────────────
template<typename T> inline T max(T a, T b) { return a > b ? a : b; }
template<typename T> inline T min(T a, T b) { return a < b ? a : b; }
template<typename T> inline T constrain(T x, T lo, T hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

// ── String ────────────────────────────────────────────────────────────────
class String {
    std::string _s;
public:
    String() {}
    String(const char* s) : _s(s ? s : "") {}
    String& operator=(const char* s)    { _s = s ? s : ""; return *this; }
    String& operator=(const String& o)  { _s = o._s;       return *this; }
    size_t length() const               { return _s.length(); }
    const char* c_str() const           { return _s.c_str(); }
};

// ── Serial ────────────────────────────────────────────────────────────────
struct HardwareSerial {
    void begin(long) {}
    template<typename T> void print(T)   {}
    template<typename T> void println(T) {}
    void println() {}
    bool available()  { return false; }
    char read()       { return 0; }
};
HardwareSerial Serial;

// ── Interrupts ────────────────────────────────────────────────────────────
inline void attachInterrupt(uint8_t, void(*)(), uint8_t) {}
inline void detachInterrupt(uint8_t) {}
inline uint8_t digitalPinToInterrupt(uint8_t pin) { return pin; }

// ── AVR register stubs ────────────────────────────────────────────────────
volatile uint8_t EIFR = 0;
#define INTF0 0
