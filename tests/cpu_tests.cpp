#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/cpu.h"
#include "../src/memory.h"

namespace {
// Tiny homegrown test runner. No external test framework is needed: every
// expect() call checks one fact, increments failures on error, and main()
// returns non-zero if anything failed.
int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cout << "FAIL: " << message << "\n";
        failures++;
    }
}

void loadProgram(Memory& mem, uint16_t start, const std::vector<uint8_t>& program)
{
    // Tests write raw machine-code bytes directly into emulated memory. This is
    // the same thing a binary loader does, just with bytes listed inline.
    for (std::size_t i = 0; i < program.size(); i++) {
        mem.write(static_cast<uint16_t>(start + i), program[i]);
    }
}

void writeVector(Memory& mem, uint16_t vector, uint16_t address)
{
    // Reset/interrupt vectors are two-byte little-endian addresses. Setting
    // $FFFC/$FFFD tells CPU::reset() where the test program starts.
    mem.write(vector, static_cast<uint8_t>(address & 0xFF));
    mem.write(static_cast<uint16_t>(vector + 1), static_cast<uint8_t>((address >> 8) & 0xFF));
}

void runUntilHalt(CPU& cpu, Memory& mem, int maxSteps = 1000)
{
    // Most test programs end with BRK. If a bug makes PC jump somewhere wrong,
    // maxSteps stops the test from looping forever.
    int steps = 0;
    while (!cpu.isHalted && steps < maxSteps) {
        cpu.step(mem);
        steps++;
    }

    expect(cpu.isHalted, "program halted before max step count");
}

struct Machine {
    // Bundle CPU and memory together so each test gets a fresh isolated
    // computer to run on.
    CPU cpu;
    Memory mem;
};

Machine boot(const std::vector<uint8_t>& program, uint16_t start = 0x0200)
{
    // Common setup for every test:
    //   clear memory
    //   copy program bytes into memory
    //   point the RESET vector at the program
    //   reset the CPU so PC is loaded like real hardware
    Machine machine;
    machine.mem.reset();
    loadProgram(machine.mem, start, program);
    writeVector(machine.mem, 0xFFFC, start);
    writeVector(machine.mem, 0xFFFE, 0x0000);
    machine.cpu.reset(machine.mem);
    return machine;
}

void testLoadStoreAndFlags()
{
    // This verifies a beginner-level CPU story:
    // LDA loads a value, BEQ looks at the zero flag, STA writes A to memory,
    // and loading $80 sets the negative flag because bit 7 is 1.
    auto machine = boot({
        0xA9, 0x00,       // LDA #$00
        0xF0, 0x02,       // BEQ +2
        0xA9, 0xFF,       // skipped
        0xA9, 0x80,       // LDA #$80
        0x85, 0x10,       // STA $10
        0x00              // BRK
    });

    runUntilHalt(machine.cpu, machine.mem);
    expect(machine.cpu.A == 0x80, "LDA/BEQ leaves A at $80");
    expect(machine.mem.read(0x0010) == 0x80, "STA zero page stores A");
    expect((machine.cpu.P & 0x80) != 0, "negative flag set for $80");
}

void testSubroutineAndStack()
{
    // JSR pushes a return address to the stack, jumps to the subroutine, and
    // RTS pops that address so execution resumes after the original JSR.
    auto machine = boot({
        0x20, 0x07, 0x02, // JSR $0207
        0x85, 0x20,       // STA $20
        0x00,             // BRK
        0xEA,             // padding
        0xA9, 0x55,       // LDA #$55
        0x60              // RTS
    });

    runUntilHalt(machine.cpu, machine.mem);
    expect(machine.cpu.A == 0x55, "JSR/RTS returns with subroutine result in A");
    expect(machine.mem.read(0x0020) == 0x55, "caller resumed after RTS");
    expect(machine.cpu.SP == 0xFA, "BRK pushes three bytes after balanced JSR/RTS stack");
}

void testArithmeticAndCarry()
{
    // Arithmetic tests are mostly flag tests. Here ADC wraps $FE + $03 to $01
    // with carry set, then SBC subtracts 1 without borrowing and lands on zero.
    auto machine = boot({
        0x18,             // CLC
        0xA9, 0xFE,       // LDA #$FE
        0x69, 0x03,       // ADC #$03
        0xE9, 0x01,       // SBC #$01
        0x00              // BRK
    });

    runUntilHalt(machine.cpu, machine.mem);
    expect(machine.cpu.A == 0x00, "ADC/SBC wrap to zero");
    expect((machine.cpu.P & 0x01) != 0, "carry flag remains set after no-borrow SBC");
    expect((machine.cpu.P & 0x02) != 0, "zero flag set after result $00");
}

void testMemoryMappedOutput()
{
    // The program does not call C++ printing code directly. It simply stores a
    // byte at $F001, and Memory turns that store into console output.
    auto machine = boot({
        0xA9, 'O',        // LDA #'O'
        0x8D, 0x01, 0xF0, // STA $F001
        0x00              // BRK
    });

    std::ostringstream output;
    machine.mem.enableConsoleIO(output);
    runUntilHalt(machine.cpu, machine.mem);
    expect(machine.mem.read(Memory::IO_OUT) == 'O', "memory-mapped output stores last byte");
    expect(output.str() == "O", "memory-mapped output writes to configured stream");
}

void testMemoryMappedInput()
{
    // This program polls $F004 first. If input is ready, it reads one byte from
    // $F005 and stores it in normal RAM at $0030.
    auto machine = boot({
        0xAD, 0x04, 0xF0, // LDA $F004
        0xF0, 0x08,       // BEQ fail
        0xAD, 0x05, 0xF0, // LDA $F005
        0x8D, 0x30, 0x00, // STA $0030
        0x00,             // BRK
        0xA9, 0xFF,       // fail: LDA #$FF
        0x00              // BRK
    });

    std::ostringstream output;
    machine.mem.enableConsoleIO(output);
    machine.mem.queueInput('Z');
    runUntilHalt(machine.cpu, machine.mem);
    expect(machine.mem.read(0x0030) == 'Z', "memory-mapped input returns queued byte");
}

void testIllegalOpcodesAreOptional()
{
    // Undocumented opcodes should be opt-in. First prove SAX* halts when the
    // feature is disabled, then prove SAX*/LAX* work after enabling it.
    auto disabled = boot({
        0xA9, 0x12,       // LDA #$12
        0xA2, 0x34,       // LDX #$34
        0x87, 0x40,       // SAX* $40
        0x00
    });

    disabled.cpu.step(disabled.mem);
    disabled.cpu.step(disabled.mem);
    // The disabled case intentionally prints "Unknown opcode"; capture it so a
    // passing test run stays clean.
    std::ostringstream ignoredOutput;
    auto* originalOutput = std::cout.rdbuf(ignoredOutput.rdbuf());
    disabled.cpu.step(disabled.mem);
    std::cout.rdbuf(originalOutput);
    expect(disabled.cpu.isHalted, "illegal opcode halts when support is disabled");
    expect(disabled.mem.read(0x0040) == 0x00, "disabled illegal opcode did not execute");

    auto enabled = boot({
        0xA9, 0x12,       // LDA #$12
        0xA2, 0x34,       // LDX #$34
        0x87, 0x40,       // SAX* $40 -> A & X
        0xA7, 0x40,       // LAX* $40 -> A and X
        0x00
    });

    enabled.cpu.enableIllegalOpcodes = true;
    runUntilHalt(enabled.cpu, enabled.mem);
    expect(enabled.mem.read(0x0040) == 0x10, "SAX* stores A & X");
    expect(enabled.cpu.A == 0x10 && enabled.cpu.X == 0x10, "LAX* loads A and X");
}
}

int main()
{
    // Add new test functions here as the emulator grows. A failing test prints
    // its message but keeps running, so one run can show multiple problems.
    testLoadStoreAndFlags();
    testSubroutineAndStack();
    testArithmeticAndCarry();
    testMemoryMappedOutput();
    testMemoryMappedInput();
    testIllegalOpcodesAreOptional();

    if (failures == 0) {
        std::cout << "\nAll CPU emulator tests passed.\n";
        return 0;
    }

    std::cout << "\n" << failures << " test failure(s).\n";
    return 1;
}
