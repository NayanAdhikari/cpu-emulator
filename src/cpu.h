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
    uint64_t cycles = 0; // Cycle Counter

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

    // Core CPU Functions
    uint8_t fetch(Memory& mem);
    void execute(uint8_t opcode, Memory& mem);

    // Helper Functions
    void setZN(uint8_t value);
    void push(Memory& mem, uint8_t value);
    uint8_t pop(Memory& mem);

    // Addressing Modes
    uint8_t immediate(Memory& mem);
    uint8_t zeroPage(Memory& mem);
    uint8_t zeroPageAddress(Memory& mem);

    // Instructions
    void LDA_IM(Memory& mem);
    void PHA(Memory& mem);
    void TAX();
    void INX();
    void DEX();
    void INY();
    void DEY();
    void BRK();

    void STA_ZP(Memory& mem);
    void LDA_ZP(Memory& mem);
    void ADC_IM(Memory& mem);
    void PLA(Memory& mem);
};