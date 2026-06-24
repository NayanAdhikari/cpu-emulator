#pragma once
#include <cstdint>
#include <array>
#include <deque>
#include <iosfwd>

constexpr uint32_t MEM_SIZE = 64 * 1024; // 64KB address space

struct Memory {
    // These addresses are reserved for tiny "devices" when I/O is enabled.
    //
    // A normal RAM address just stores a byte. A memory-mapped I/O address
    // still looks like memory to the CPU, but reading or writing it can trigger
    // outside-world behavior. This is how many simple computers talk to
    // keyboards, screens, timers, sound chips, and other hardware.
    static constexpr uint16_t IO_OUT = 0xF001;
    static constexpr uint16_t IO_IN_READY = 0xF004;
    static constexpr uint16_t IO_IN = 0xF005;

    // Memory is the CPU's large byte-addressable storage.
    //
    // "64KB" means 65,536 individual byte slots. Because the 6502 has a
    // 16-bit address bus, it can name exactly 65,536 addresses: $0000-$FFFF.
    // The CPU does not know about this std::array; it only asks read/write for
    // bytes at 16-bit addresses.
    // The 6502 sees one flat 16-bit address space: $0000-$FFFF.
    // This array is the emulator's RAM/ROM backing store for that full range.
    std::array<uint8_t, MEM_SIZE> data{};

    // Memory-mapped I/O lets normal CPU load/store instructions talk to simple
    // devices. The CPU still thinks it is reading/writing memory; Memory
    // decides that special addresses should do something extra.
    //
    // ioEnabled keeps this feature optional. When it is false, $F001/$F004/$F005
    // are just normal memory addresses.
    bool ioEnabled = false;

    // output points at the stream where console output should go. In the real
    // runner this is std::cout; in tests it can be a string stream so output
    // can be checked without printing noisy text.
    std::ostream* output = nullptr;

    // Input is queued so a test or command-line option can preload characters.
    // mutable allows read() to consume input while still being a logically
    // "read-only" memory operation from the CPU's point of view.
    mutable std::deque<uint8_t> inputBuffer{};

    // Clear all memory to zero. CPU::reset() intentionally does not call this,
    // because real CPU reset changes registers but does not erase memory.
    void reset();

    // Turn on the simple console device. After this:
    //   mem.write($F001, 'A') prints A
    //   mem.read($F004) reports whether input is waiting
    //   mem.read($F005) consumes one input byte
    void enableConsoleIO(std::ostream& out);

    // Queue input bytes that a 6502 program can later read from $F005.
    void queueInput(uint8_t value);
    void queueInput(const char* text);

    // Every address is uint16_t, so reads/writes naturally wrap inside 64KB.
    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);
};
