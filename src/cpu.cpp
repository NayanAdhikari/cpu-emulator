#include "cpu.h"
#include <iostream>

void CPU::reset(Memory& mem) {
    A = X = Y = 0;
    SP = 0xFF;
    PC = 0x0200; // Common start address for 6502 programs
    mem.reset();
}

uint8_t CPU::fetch(Memory& mem) {
    return mem.read(PC++); // Read byte at PC, then increment
}

void CPU::execute(uint8_t opcode, Memory& mem) {
    switch (opcode) {

        case 0xA9: { // LDA Immediate — Load A with next byte
            A = fetch(mem);
            flags.Z = (A == 0);
            flags.N = (A & 0x80) != 0;
            break;
        }

        case 0xAA: { // TAX — Transfer A to X
            X = A;
            flags.Z = (X == 0);
            flags.N = (X & 0x80) != 0;
            break;
        }

        case 0xE8: { // INX — Increment X
            X++;
            flags.Z = (X == 0);
            flags.N = (X & 0x80) != 0;
            break;
        }

        case 0x00: { // BRK — Break / halt
            std::cout << "[BRK] Program halted.\n";
            isHalted = true; // Freeze the PC
            break;
        }

        default:
            std::cout << "Unknown opcode: 0x" 
                      << std::hex << (int)opcode << "\n";
            break;
    }
}

void CPU::step(Memory& mem) {
    uint8_t opcode = fetch(mem);
    execute(opcode, mem);
}