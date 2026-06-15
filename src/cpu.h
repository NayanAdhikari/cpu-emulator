#pragma once

#include <cstdint>
#include <string>
#include "memory.h"

// CPU is the part of the emulator that acts like the processor chip.
//
// A real CPU repeatedly does this:
//   1. Look at the memory address stored in PC.
//   2. Read one byte there. That byte is the opcode, meaning "which instruction?"
//   3. Read any extra operand bytes the instruction needs.
//   4. Change registers and/or memory based on that instruction.
//
// This struct stores the CPU's tiny internal pieces of state, then cpu.cpp
// implements the fetch -> decode -> execute loop.
struct CPU {
    // The runner uses this to stop executing after BRK or an unknown opcode.
    bool isHalted = false;

    // Real 6502 hardware jumps through the IRQ/BRK vector after BRK.
    // For small test programs, halting on BRK is much more convenient.
    bool haltOnBreak = true;

    // Registers are tiny storage locations inside the CPU itself.
    // They are much faster and smaller than memory.
    //
    // A: "Accumulator." Most math and logic instructions use this register.
    // X/Y: "Index" registers. Commonly used for counting and offset addressing.
    // SP: "Stack Pointer." Points to the next stack location inside $0100-$01FF.
    // PC: "Program Counter." The memory address of the next instruction byte.
    uint8_t A = 0;
    uint8_t X = 0;
    uint8_t Y = 0;
    uint8_t SP = 0;
    uint16_t PC = 0;

    // Total elapsed CPU cycles. Each instruction adds its base cycles, and
    // some addressing modes add extra cycles for page crossing or branches.
    uint64_t cycles = 0;

    // Processor status register. This one byte stores several yes/no flags.
    // Instructions set these flags so later branch instructions can ask
    // questions like "was the last result zero?" or "did addition overflow?"
    // Bit layout: N V - B D I Z C
    //             7 6 5 4 3 2 1 0
    //
    // N: negative result, V: signed overflow, B: BRK/software interrupt,
    // D: decimal mode, I: IRQ interrupt disable, Z: zero result, C: carry/borrow.
    uint8_t P = 0x20;

    // RESET is how the CPU starts. Instead of hardcoding a start address, the
    // 6502 reads two bytes from $FFFC/$FFFD and uses them as the starting PC.
    void reset(Memory& mem);

    // Execute one complete fetch/decode/execute cycle.
    void step(Memory& mem);

    // Interrupts are hardware "pause what you are doing and jump here" events.
    // NMI always fires; IRQ is ignored when the I flag is set.
    void nmi(Memory& mem);
    void irq(Memory& mem);

    // Utilities used by the runner/debugger. disassemble() does not execute;
    // it only formats the instruction bytes at an address.
    std::string disassemble(const Memory& mem, uint16_t address, uint8_t* bytesUsed = nullptr) const;
    std::string registers() const;

    // Addressing modes answer "where does this instruction get its value?"
    // Example: LDA #$42 loads the literal number $42, while LDA $2000 loads
    // the byte stored in memory at address $2000. Same instruction idea,
    // different way of finding the operand.
    enum class AddressMode {
        Implied,
        Accumulator,
        Immediate,
        ZeroPage,
        ZeroPageX,
        ZeroPageY,
        Relative,
        Absolute,
        AbsoluteX,
        AbsoluteY,
        Indirect,
        IndexedIndirect,
        IndirectIndexed,
    };

private:
    // Individual bits inside P. Each flag is one bit in the status register.
    // Giving them names keeps the code readable.
    enum StatusFlag : uint8_t {
        CARRY = 0x01,
        ZERO = 0x02,
        IRQ_DISABLE = 0x04,
        DECIMAL = 0x08,
        BREAK = 0x10,
        UNUSED = 0x20,
        OVERFLOW = 0x40,
        NEGATIVE = 0x80,
    };

    bool getFlag(StatusFlag flag) const;
    void setFlag(StatusFlag flag, bool value);

    // Almost every load, transfer, math, and shift instruction updates:
    //   Z if the result is 0
    //   N if bit 7 of the result is 1
    // This helper prevents repeated flag code.
    void setZN(uint8_t value);

    // fetch() reads the byte at PC, then advances PC to the next byte.
    // This is how the CPU walks through program memory one byte at a time.
    uint8_t fetch(Memory& mem);

    // fetch16() reads two bytes and combines them into a 16-bit address.
    // 6502 addresses are little-endian: low byte first, high byte second.
    uint16_t fetch16(Memory& mem);

    // read16Bug() models the famous 6502 JMP ($xxFF) page-wrap bug.
    uint16_t read16(const Memory& mem, uint16_t address) const;
    uint16_t read16Bug(const Memory& mem, uint16_t address) const;

    // The stack is a last-in, first-out scratch area used for return addresses,
    // saved CPU state, and push/pop instructions. On the 6502 it always lives
    // in memory page 1: $0100-$01FF.
    void push(Memory& mem, uint8_t value);
    uint8_t pop(Memory& mem);
    void push16(Memory& mem, uint16_t value);
    uint16_t pop16(Memory& mem);

    // Shared addressing helpers. pageCrossed tells the caller whether an
    // absolute indexed or indirect indexed read crossed a 256-byte page.
    uint16_t operandAddress(Memory& mem, AddressMode mode, bool* pageCrossed = nullptr);
    uint8_t readOperand(Memory& mem, AddressMode mode, bool* pageCrossed = nullptr);
    void writeOperand(Memory& mem, AddressMode mode, uint8_t value);

    // Shared instruction helpers for behavior that appears in many opcodes.
    void serviceInterrupt(Memory& mem, uint16_t vector, bool breakFlag);
    void branch(Memory& mem, bool condition);
    void compare(uint8_t reg, uint8_t value);
    void adc(uint8_t value);
    void sbc(uint8_t value);

    void execute(uint8_t opcode, Memory& mem);
};
