#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <cmath>     // float overloads of fabs() etc., as on AVR
#include <stdio.h>

// ── Constants ─────────────────────────────────────────────────────────────
#define HIGH         1
#define LOW          0
#define INPUT_PULLUP 0x02
#define OUTPUT       0x01
#define FALLING      2
#define RISING       3

// Analog pin numbers (Arduino Nano / ATmega328P)
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define A6 20
#define A7 21

// ── Mock time ─────────────────────────────────────────────────────────────
uint32_t mock_millis = 0;
uint32_t mock_micros = 0;
inline uint32_t millis() { return mock_millis; }
inline uint32_t micros() { return mock_micros; }
inline void delay(uint32_t) {}

// ── Mock GPIO ─────────────────────────────────────────────────────────────
uint8_t mock_pin[22] = {};
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t val) { if (pin < 22) mock_pin[pin] = val; }
inline int  digitalRead (uint8_t pin)              { return (pin < 22) ? mock_pin[pin] : LOW; }

// ── Mock ADC ──────────────────────────────────────────────────────────────
// mock_adc[n] is the value analogRead(An) returns (0..1023).
uint16_t mock_adc[8] = {};
uint32_t mock_adc_reads = 0;
inline int analogRead(uint8_t pin) {
    mock_adc_reads++;
    return (pin >= A0 && pin <= A7) ? mock_adc[pin - A0] : 0;
}

// ── Math ──────────────────────────────────────────────────────────────────
template<typename T> inline T max(T a, T b) { return a > b ? a : b; }
template<typename T> inline T min(T a, T b) { return a < b ? a : b; }
template<typename T> inline T constrain(T x, T lo, T hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

// ── Flash strings ────────────────────────────────────────────────────────
// On AVR, F("text") keeps the literal in flash. Natively it is just the text.
#define F(x) (x)

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
inline void noInterrupts() {}
inline void interrupts() {}

// ── AVR register stubs ────────────────────────────────────────────────────
volatile uint8_t EIFR = 0;
#define INTF0 0
