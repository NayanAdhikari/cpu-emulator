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

int main() {
    Memory mem;
    CPU cpu;

    cpu.reset(mem);

    loadProgram(mem, 0x0200, {
        0xA9, 0x42, // LDA #$42
        0x48,       // PHA
        0xA9, 0x00, // LDA #0
        0x68,       // PLA
        0x00        // BRK
    });

    while (!cpu.isHalted)
    {
        cpu.step(mem);

        std::cout << std::hex
                  << "A=0x" << (int)cpu.A
                  << " X=0x" << (int)cpu.X
                  << " Y=0x" << (int)cpu.Y
                  << " SP=0x" << (int)cpu.SP
                  << " PC=0x" << cpu.PC
                  << " CYC=" << std::dec << cpu.cycles
                  << "\n";
    }

    return 0;
}