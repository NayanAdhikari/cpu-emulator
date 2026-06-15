#pragma once
#include <cstdint>
#include <array>

constexpr uint32_t MEM_SIZE = 64 * 1024; // 64KB address space

struct Memory {
    // Memory is the CPU's large byte-addressable storage.
    //
    // "64KB" means 65,536 individual byte slots. Because the 6502 has a
    // 16-bit address bus, it can name exactly 65,536 addresses: $0000-$FFFF.
    // The CPU does not know about this std::array; it only asks read/write for
    // bytes at 16-bit addresses.
    // The 6502 sees one flat 16-bit address space: $0000-$FFFF.
    // This array is the emulator's RAM/ROM backing store for that full range.
    std::array<uint8_t, MEM_SIZE> data{};

    // Clear all memory to zero. CPU::reset() intentionally does not call this,
    // because real CPU reset changes registers but does not erase memory.
    void reset();

    // Every address is uint16_t, so reads/writes naturally wrap inside 64KB.
    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);
};
