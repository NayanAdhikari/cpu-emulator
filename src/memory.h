#pragma once
#include <cstdint>
#include <array>

constexpr uint32_t MEM_SIZE = 64 * 1024; // 64KB address space

struct Memory {
    std::array<uint8_t, MEM_SIZE> data{};

    void reset();
    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);
};