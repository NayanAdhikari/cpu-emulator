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

uint8_t CPU::immediate(Memory& mem)
{
    return fetch(mem);
}

uint8_t CPU::zeroPage(Memory& mem)
{
    uint8_t address = fetch(mem);

    return mem.read(address);
}

uint8_t CPU::zeroPageAddress(Memory& mem)
{
    return fetch(mem);
}

void CPU::execute(uint8_t opcode, Memory& mem)
{
    switch(opcode)
    {
        case 0xA9:
            LDA_IM(mem);
            break;

        case 0xAA:
            TAX();
            break;

        case 0xE8:
            INX();
            break;

        case 0xCA:
            DEX();
            break;

        case 0xC8:
            INY();
            break;

        case 0x88:
            DEY();
            break;

        case 0x85:
            STA_ZP(mem);
            break;

        case 0xA5:
            LDA_ZP(mem);
            break;

        case 0x69:
            ADC_IM(mem);
            break;
        
        case 0x48:
            PHA(mem);
            break;
            
        case 0x68:
            PLA(mem);
            break;

        case 0x00:
            BRK();
            break;

        default:
            std::cout << "Unknown opcode: 0x"
                      << std::hex << (int)opcode << "\n";
            break;
    }
}

void CPU::push(Memory& mem, uint8_t value)
{
    mem.write(0x0100 + SP, value);
    SP--;
}

uint8_t CPU::pop(Memory& mem)
{
    SP++;
    return mem.read(0x0100 + SP);
}

void CPU::LDA_IM(Memory& mem)
{
    A = immediate(mem);

    setZN(A);
}

void CPU::TAX()
{
    X = A;
    setZN(X);
}

void CPU::INX()
{
    X++;
    setZN(X);
}

void CPU::DEX()
{
    X--;
    setZN(X);
}

void CPU::INY()
{
    Y++;
    setZN(Y);
}

void CPU::DEY()
{
    Y--;
    setZN(Y);
}

void CPU::BRK()
{
    std::cout << "[BRK] Program halted.\n";
    isHalted = true;
}

void CPU::STA_ZP(Memory& mem)
{
    uint8_t address = zeroPageAddress(mem);
    mem.write(address, A);
}

void CPU::LDA_ZP(Memory& mem)
{
    A = zeroPage(mem);

    setZN(A);
}

void CPU::ADC_IM(Memory& mem)
{
    uint8_t value = immediate(mem);

    uint16_t result = A + value + flags.C;

    flags.C = result > 0xFF;

    flags.V =
        (~(A ^ value) & (A ^ result) & 0x80) != 0;

    A = result & 0xFF;

    setZN(A);
}

void CPU::PHA(Memory& mem)
{
    push(mem, A);
}

void CPU::PLA(Memory& mem)
{
    A = pop(mem);

    setZN(A);
}

void CPU::setZN(uint8_t value)
{
    flags.Z = (value == 0);
    flags.N = (value & 0x80) != 0;
}

void CPU::step(Memory& mem)
{
    uint8_t opcode = fetch(mem);

    execute(opcode, mem);

    cycles++;
}