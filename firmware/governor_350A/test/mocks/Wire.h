#pragma once
#include <stdint.h>

// mock_wire_ack: true = a device answers at the probed address (OLED fitted).
bool     mock_wire_ack = false;
uint32_t mock_wire_transmissions = 0;

class TwoWire {
public:
    void    begin() {}
    void    beginTransmission(uint8_t) {}
    uint8_t endTransmission() { mock_wire_transmissions++; return mock_wire_ack ? 0 : 2; }
};
TwoWire Wire;
