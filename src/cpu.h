#pragma once
#include <cstdint>
#include "memory.h"

struct CPU {
    bool isHalted = false;
    // Registers
    uint8_t  A  = 0;   // Accumulator
    uint8_t  X  = 0;   // Index Register X
    uint8_t  Y  = 0;   // Index Register Y
    uint8_t  SP = 0;   // Stack Pointer
    uint16_t PC = 0;   // Program Counter

    // Status Flags (packed into one byte)
    struct Flags {
        uint8_t C : 1; // Carry
        uint8_t Z : 1; // Zero
        uint8_t I : 1; // Interrupt Disable
        uint8_t D : 1; // Decimal Mode
        uint8_t B : 1; // Break Command
        uint8_t V : 1; // Overflow
        uint8_t N : 1; // Negative
    } flags{};

    void reset(Memory& mem);
    void step(Memory& mem);   // One fetch-decode-execute cycle

private:
    uint8_t fetch(Memory& mem);
    void execute(uint8_t opcode, Memory& mem);
};