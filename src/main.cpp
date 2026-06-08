#include <iostream>
#include <vector>
#include "cpu.h"
#include "memory.h"

void loadProgram(Memory& mem, uint16_t start, const std::vector<uint8_t>& program)
{
    for (size_t i = 0; i < program.size(); i++)
    {
        mem.write(start + i, program[i]);
    }
}

int main()
{
    Memory mem;
    CPU cpu;

    loadProgram(mem, 0x0200, {
    0xA2, 0x01,        // LDX #$01
    0xA9, 0x42,        // LDA #$42
    0x95, 0x10,        // STA $10,X   — stores $42 at $0011
    0xA9, 0x00,        // LDA #$00    — clear A
    0xB5, 0x10,        // LDA $10,X   — load from $0011, should get $42 back
    0x00               // BRK
});

    mem.write(0xFFFC, 0x00);
    mem.write(0xFFFD, 0x02);

    cpu.reset(mem);  // <-- must be LAST, after everything is written

    while (!cpu.isHalted)
    {
        cpu.step(mem);

        std::cout << std::hex
                  << "A=0x"   << (int)cpu.A
                  << " X=0x"  << (int)cpu.X
                  << " Y=0x"  << (int)cpu.Y
                  << " SP=0x" << (int)cpu.SP
                  << " PC=0x" << (int)cpu.PC
                  << " P=0x"  << (int)cpu.P
                  << " CYC="  << std::dec << cpu.cycles
                  << "\n";
    }

    return 0;
}