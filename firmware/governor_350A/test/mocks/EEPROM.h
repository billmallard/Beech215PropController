#pragma once
#include <stdint.h>
#include <string.h>

struct EEPROMClass {
    uint8_t data[1024] = {};

    uint8_t read(int idx)              { return data[idx]; }
    void    write(int idx, uint8_t val){ data[idx] = val; }

    template<typename T>
    T& get(int idx, T& t) {
        memcpy(&t, &data[idx], sizeof(T));
        return t;
    }
    template<typename T>
    const T& put(int idx, const T& t) {
        memcpy(&data[idx], &t, sizeof(T));
        return t;
    }
};
EEPROMClass EEPROM;
