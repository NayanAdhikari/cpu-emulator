#include <iostream>
#include "cpu.h"
#include "memory.h"

int main() {
    Memory mem;
    CPU cpu;
    cpu.reset(mem);

    // Write a tiny program into memory manually:
    // LDA #0x05  → TAX  → INX  → BRK
    mem.write(0x0200, 0xA9); // LDA
    mem.write(0x0201, 0x05); // #$05
    mem.write(0x0202, 0xAA); // TAX
    mem.write(0x0203, 0xE8); // INX
    mem.write(0x0204, 0x00); // BRK

    // Run until BRK
    for (int i = 0; i < 10; i++) {
        cpu.step(mem);
        std::cout << "A=" << (int)cpu.A 
                  << " X=" << (int)cpu.X 
                  << " PC=" << std::hex << cpu.PC << "\n";
        if (cpu.PC == 0x0204) break;
    }

    return 0;
}