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
    mem.write(0x0200, 0xA9); // LDA #10
    mem.write(0x0201, 0x0A);
    mem.write(0x0202, 0x85); // STA $20 (store A at address 0x20)
    mem.write(0x0203, 0x20);
    mem.write(0x0204, 0x69); // ADC #5 (add 5 to A)
    mem.write(0x0205, 0x05);
    mem.write(0x0206, 0xA5); // LDA $20 (load from address 0x20)
    mem.write(0x0207, 0x20);
    mem.write(0x0208, 0x00); // BRK

    // Run until BRK
   while (!cpu.isHalted) {
    cpu.step(mem);
    if (!cpu.isHalted) {
        std::cout << "A=" << (int)cpu.A 
                  << " X=" << (int)cpu.X 
                  << " PC=" << std::hex << cpu.PC << "\n";
    }
}

    return 0;
}