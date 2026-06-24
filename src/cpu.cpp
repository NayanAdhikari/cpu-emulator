#include "cpu.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
// A 6502 instruction is represented by an opcode byte. Because a byte can hold
// 256 possible values, the CPU can see opcodes $00 through $FF.
//
// This table is indexed directly by opcode value. For example, opcode $A9
// means LDA immediate and takes 2 cycles, so CYCLE_TABLE[0xA9] is 2.
// One entry per possible opcode byte. Legal opcodes contain their base cycle
// count; illegal/unimplemented opcodes are zero and halt in execute().
// Extra cycles for page crossings and taken branches are added at runtime.
const uint8_t CYCLE_TABLE[256] = {
    7, 6, 0, 0, 0, 3, 5, 0, 3, 2, 2, 0, 0, 4, 6, 0,
    2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
    6, 6, 0, 0, 3, 3, 5, 0, 4, 2, 2, 0, 4, 4, 6, 0,
    2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
    6, 6, 0, 0, 0, 3, 5, 0, 3, 2, 2, 0, 3, 4, 6, 0,
    2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
    6, 6, 0, 0, 0, 3, 5, 0, 4, 2, 2, 0, 5, 4, 6, 0,
    2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
    0, 6, 0, 0, 3, 3, 3, 0, 2, 0, 2, 0, 4, 4, 4, 0,
    2, 6, 0, 0, 4, 4, 4, 0, 2, 5, 2, 0, 0, 5, 0, 0,
    2, 6, 2, 0, 3, 3, 3, 0, 2, 2, 2, 0, 4, 4, 4, 0,
    2, 5, 0, 0, 4, 4, 4, 0, 2, 4, 2, 0, 4, 4, 4, 0,
    2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,
    2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
    2, 6, 0, 0, 3, 3, 5, 0, 2, 2, 2, 0, 4, 4, 6, 0,
    2, 5, 0, 0, 0, 4, 6, 0, 2, 4, 0, 0, 0, 4, 7, 0,
};

struct InstructionInfo {
    // Human-readable name, such as "LDA" or "JSR".
    const char* mnemonic;

    // How this instruction finds its operand.
    CPU::AddressMode mode;

    // Total instruction length in memory: opcode byte plus operand bytes.
    uint8_t bytes;
};

// The disassembler needs to know how many bytes an instruction occupies.
// The CPU execution path gets this naturally by fetching operands, but a
// disassembler has to look without changing PC.
uint8_t bytesFor(CPU::AddressMode mode)
{
    switch (mode) {
    case CPU::AddressMode::Implied:
    case CPU::AddressMode::Accumulator:
        return 1;
    case CPU::AddressMode::Immediate:
    case CPU::AddressMode::ZeroPage:
    case CPU::AddressMode::ZeroPageX:
    case CPU::AddressMode::ZeroPageY:
    case CPU::AddressMode::Relative:
    case CPU::AddressMode::IndexedIndirect:
    case CPU::AddressMode::IndirectIndexed:
        return 2;
    case CPU::AddressMode::Absolute:
    case CPU::AddressMode::AbsoluteX:
    case CPU::AddressMode::AbsoluteY:
    case CPU::AddressMode::Indirect:
        return 3;
    }

    return 1;
}

InstructionInfo decodeInfo(uint8_t opcode)
{
    using Mode = CPU::AddressMode;

    // Decoding means converting a raw opcode byte into meaning.
    // Example: byte $A9 becomes "LDA using immediate addressing, 2 bytes long."
    //
    // The long switch below is basically the CPU's instruction manual written
    // as C++ data.
    // This table is shared by the CPU and the disassembler. Keeping mnemonic,
    // addressing mode, and byte length together makes debugger output match
    // the same opcode interpretation that execute() uses.
    switch (opcode) {
    case 0x69: return {"ADC", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x65: return {"ADC", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x75: return {"ADC", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x6D: return {"ADC", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x7D: return {"ADC", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0x79: return {"ADC", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x61: return {"ADC", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x71: return {"ADC", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0x29: return {"AND", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x25: return {"AND", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x35: return {"AND", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x2D: return {"AND", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x3D: return {"AND", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0x39: return {"AND", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x21: return {"AND", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x31: return {"AND", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0x0A: return {"ASL", Mode::Accumulator, bytesFor(Mode::Accumulator)};
    case 0x06: return {"ASL", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x16: return {"ASL", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x0E: return {"ASL", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x1E: return {"ASL", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x90: return {"BCC", Mode::Relative, bytesFor(Mode::Relative)};
    case 0xB0: return {"BCS", Mode::Relative, bytesFor(Mode::Relative)};
    case 0xF0: return {"BEQ", Mode::Relative, bytesFor(Mode::Relative)};
    case 0x24: return {"BIT", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x2C: return {"BIT", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x30: return {"BMI", Mode::Relative, bytesFor(Mode::Relative)};
    case 0xD0: return {"BNE", Mode::Relative, bytesFor(Mode::Relative)};
    case 0x10: return {"BPL", Mode::Relative, bytesFor(Mode::Relative)};
    case 0x00: return {"BRK", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x50: return {"BVC", Mode::Relative, bytesFor(Mode::Relative)};
    case 0x70: return {"BVS", Mode::Relative, bytesFor(Mode::Relative)};

    case 0x18: return {"CLC", Mode::Implied, bytesFor(Mode::Implied)};
    case 0xD8: return {"CLD", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x58: return {"CLI", Mode::Implied, bytesFor(Mode::Implied)};
    case 0xB8: return {"CLV", Mode::Implied, bytesFor(Mode::Implied)};

    case 0xC9: return {"CMP", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xC5: return {"CMP", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xD5: return {"CMP", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xCD: return {"CMP", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xDD: return {"CMP", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0xD9: return {"CMP", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0xC1: return {"CMP", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0xD1: return {"CMP", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0xE0: return {"CPX", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xE4: return {"CPX", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xEC: return {"CPX", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xC0: return {"CPY", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xC4: return {"CPY", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xCC: return {"CPY", Mode::Absolute, bytesFor(Mode::Absolute)};

    case 0xC6: return {"DEC", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xD6: return {"DEC", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xCE: return {"DEC", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xDE: return {"DEC", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0xCA: return {"DEX", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x88: return {"DEY", Mode::Implied, bytesFor(Mode::Implied)};

    case 0x49: return {"EOR", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x45: return {"EOR", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x55: return {"EOR", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x4D: return {"EOR", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x5D: return {"EOR", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0x59: return {"EOR", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x41: return {"EOR", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x51: return {"EOR", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0xE6: return {"INC", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xF6: return {"INC", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xEE: return {"INC", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xFE: return {"INC", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0xE8: return {"INX", Mode::Implied, bytesFor(Mode::Implied)};
    case 0xC8: return {"INY", Mode::Implied, bytesFor(Mode::Implied)};

    case 0x4C: return {"JMP", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x6C: return {"JMP", Mode::Indirect, bytesFor(Mode::Indirect)};
    case 0x20: return {"JSR", Mode::Absolute, bytesFor(Mode::Absolute)};

    case 0xA9: return {"LDA", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xA5: return {"LDA", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xB5: return {"LDA", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xAD: return {"LDA", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xBD: return {"LDA", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0xB9: return {"LDA", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0xA1: return {"LDA", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0xB1: return {"LDA", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0xA2: return {"LDX", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xA6: return {"LDX", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xB6: return {"LDX", Mode::ZeroPageY, bytesFor(Mode::ZeroPageY)};
    case 0xAE: return {"LDX", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xBE: return {"LDX", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};

    case 0xA0: return {"LDY", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xA4: return {"LDY", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xB4: return {"LDY", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xAC: return {"LDY", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xBC: return {"LDY", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x4A: return {"LSR", Mode::Accumulator, bytesFor(Mode::Accumulator)};
    case 0x46: return {"LSR", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x56: return {"LSR", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x4E: return {"LSR", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x5E: return {"LSR", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0xEA: return {"NOP", Mode::Implied, bytesFor(Mode::Implied)};

    case 0x09: return {"ORA", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x05: return {"ORA", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x15: return {"ORA", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x0D: return {"ORA", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x1D: return {"ORA", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0x19: return {"ORA", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x01: return {"ORA", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x11: return {"ORA", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0x48: return {"PHA", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x08: return {"PHP", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x68: return {"PLA", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x28: return {"PLP", Mode::Implied, bytesFor(Mode::Implied)};

    case 0x2A: return {"ROL", Mode::Accumulator, bytesFor(Mode::Accumulator)};
    case 0x26: return {"ROL", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x36: return {"ROL", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x2E: return {"ROL", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x3E: return {"ROL", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x6A: return {"ROR", Mode::Accumulator, bytesFor(Mode::Accumulator)};
    case 0x66: return {"ROR", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x76: return {"ROR", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x6E: return {"ROR", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x7E: return {"ROR", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x40: return {"RTI", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x60: return {"RTS", Mode::Implied, bytesFor(Mode::Implied)};

    case 0xE9: return {"SBC", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xE5: return {"SBC", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xF5: return {"SBC", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xED: return {"SBC", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xFD: return {"SBC", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0xF9: return {"SBC", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0xE1: return {"SBC", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0xF1: return {"SBC", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0x38: return {"SEC", Mode::Implied, bytesFor(Mode::Implied)};
    case 0xF8: return {"SED", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x78: return {"SEI", Mode::Implied, bytesFor(Mode::Implied)};

    case 0x85: return {"STA", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x95: return {"STA", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x8D: return {"STA", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x9D: return {"STA", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};
    case 0x99: return {"STA", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x81: return {"STA", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x91: return {"STA", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};

    case 0x86: return {"STX", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x96: return {"STX", Mode::ZeroPageY, bytesFor(Mode::ZeroPageY)};
    case 0x8E: return {"STX", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x84: return {"STY", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x94: return {"STY", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x8C: return {"STY", Mode::Absolute, bytesFor(Mode::Absolute)};

    case 0xAA: return {"TAX", Mode::Implied, bytesFor(Mode::Implied)};
    case 0xA8: return {"TAY", Mode::Implied, bytesFor(Mode::Implied)};
    case 0xBA: return {"TSX", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x8A: return {"TXA", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x9A: return {"TXS", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x98: return {"TYA", Mode::Implied, bytesFor(Mode::Implied)};

    // A trailing * marks undocumented opcodes. They are decoded so the
    // disassembler can show useful names even though execution requires
    // enableIllegalOpcodes to be true.
    case 0x0B: case 0x2B: return {"ANC*", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x4B: return {"ALR*", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x6B: return {"ARR*", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xCB: return {"AXS*", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0xEB: return {"SBC*", Mode::Immediate, bytesFor(Mode::Immediate)};

    case 0xA3: return {"LAX*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0xA7: return {"LAX*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xAF: return {"LAX*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xB3: return {"LAX*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0xB7: return {"LAX*", Mode::ZeroPageY, bytesFor(Mode::ZeroPageY)};
    case 0xBF: return {"LAX*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};

    case 0x83: return {"SAX*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x87: return {"SAX*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x8F: return {"SAX*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x97: return {"SAX*", Mode::ZeroPageY, bytesFor(Mode::ZeroPageY)};

    case 0x03: return {"SLO*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x07: return {"SLO*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x0F: return {"SLO*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x13: return {"SLO*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0x17: return {"SLO*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x1B: return {"SLO*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x1F: return {"SLO*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x23: return {"RLA*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x27: return {"RLA*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x2F: return {"RLA*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x33: return {"RLA*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0x37: return {"RLA*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x3B: return {"RLA*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x3F: return {"RLA*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x43: return {"SRE*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x47: return {"SRE*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x4F: return {"SRE*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x53: return {"SRE*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0x57: return {"SRE*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x5B: return {"SRE*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x5F: return {"SRE*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x63: return {"RRA*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0x67: return {"RRA*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x6F: return {"RRA*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x73: return {"RRA*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0x77: return {"RRA*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x7B: return {"RRA*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0x7F: return {"RRA*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0xC3: return {"DCP*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0xC7: return {"DCP*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xCF: return {"DCP*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xD3: return {"DCP*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0xD7: return {"DCP*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xDB: return {"DCP*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0xDF: return {"DCP*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0xE3: return {"ISB*", Mode::IndexedIndirect, bytesFor(Mode::IndexedIndirect)};
    case 0xE7: return {"ISB*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0xEF: return {"ISB*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0xF3: return {"ISB*", Mode::IndirectIndexed, bytesFor(Mode::IndirectIndexed)};
    case 0xF7: return {"ISB*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0xFB: return {"ISB*", Mode::AbsoluteY, bytesFor(Mode::AbsoluteY)};
    case 0xFF: return {"ISB*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
        return {"NOP*", Mode::Implied, bytesFor(Mode::Implied)};
    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
        return {"NOP*", Mode::Immediate, bytesFor(Mode::Immediate)};
    case 0x04: case 0x44: case 0x64:
        return {"NOP*", Mode::ZeroPage, bytesFor(Mode::ZeroPage)};
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
        return {"NOP*", Mode::ZeroPageX, bytesFor(Mode::ZeroPageX)};
    case 0x0C:
        return {"NOP*", Mode::Absolute, bytesFor(Mode::Absolute)};
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        return {"NOP*", Mode::AbsoluteX, bytesFor(Mode::AbsoluteX)};

    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
    case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
        return {"KIL*", Mode::Implied, bytesFor(Mode::Implied)};
    }

    return {"???", Mode::Implied, 1};
}

std::string hexByte(uint8_t value)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(value);
    return out.str();
}

std::string hexWord(uint16_t value)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << value;
    return out.str();
}

bool addsPageCycle(uint8_t opcode)
{
    // Only read-like indexed instructions get an extra cycle when address
    // calculation crosses a page boundary. Stores do not.
    switch (opcode) {
    case 0x1D: case 0x19: case 0x11:
    case 0x3D: case 0x39: case 0x31:
    case 0x5D: case 0x59: case 0x51:
    case 0x7D: case 0x79: case 0x71:
    case 0xBD: case 0xB9: case 0xB1:
    case 0xBE: case 0xBC:
    case 0xDD: case 0xD9: case 0xD1:
    case 0xFD: case 0xF9: case 0xF1:
        return true;
    default:
        return false;
    }
}

uint8_t illegalOpcodeCycles(uint8_t opcode)
{
    // Undocumented opcodes still take real CPU time. This table gives their
    // base cycle counts, separate from CYCLE_TABLE so official opcode timing
    // stays easy to audit.
    switch (opcode) {
    case 0x0B: case 0x2B: case 0x4B: case 0x6B: case 0x80:
    case 0x82: case 0x89: case 0xC2: case 0xCB: case 0xE2: case 0xEB:
        return 2;

    case 0x04: case 0x07: case 0x1A: case 0x27: case 0x3A:
    case 0x44: case 0x47: case 0x5A: case 0x64: case 0x67:
    case 0x7A: case 0x87: case 0xA7: case 0xC7: case 0xDA:
    case 0xE7: case 0xFA:
        return 3;

    case 0x0C: case 0x0F: case 0x14: case 0x17: case 0x2F:
    case 0x34: case 0x37: case 0x4F: case 0x54: case 0x57:
    case 0x6F: case 0x74: case 0x77: case 0x8F: case 0x97:
    case 0xAF: case 0xB7: case 0xCF: case 0xD4: case 0xD7:
    case 0xEF: case 0xF4: case 0xF7:
        return 4;

    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        return 4;

    case 0xBF:
        return 4;

    case 0xB3:
        return 5;

    case 0xA3:
        return 6;

    case 0x03: case 0x1B: case 0x1F: case 0x23: case 0x3B:
    case 0x3F: case 0x43: case 0x5B: case 0x5F: case 0x63:
    case 0x7B: case 0x7F: case 0x83: case 0xC3: case 0xDB:
    case 0xDF: case 0xE3: case 0xFB: case 0xFF:
        return 7;

    case 0x13: case 0x33: case 0x53: case 0x73: case 0x93:
    case 0xD3: case 0xF3:
        return 8;

    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
    case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
        return 0;
    }

    return 0;
}

bool illegalNopAddsPageCycle(uint8_t opcode)
{
    // Some unofficial NOPs still perform an address calculation. The absolute,X
    // versions can spend an extra cycle when that calculation crosses a page.
    switch (opcode) {
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        return true;
    default:
        return false;
    }
}
}

void CPU::reset(Memory& mem)
{
    // Think of reset as powering on the CPU.
    // Registers start in a known state, but the program start address comes
    // from memory. That lets different programs choose their own entry point.
    // RESET does not clear RAM. Hardware leaves memory alone and only puts the
    // CPU registers into startup state, then loads PC from the reset vector.
    A = X = Y = 0;
    SP = 0xFD;
    P = UNUSED | IRQ_DISABLE;
    PC = read16(mem, 0xFFFC);
    cycles = 0;
    isHalted = false;
}

void CPU::step(Memory& mem)
{
    if (isHalted) {
        return;
    }

    // The basic CPU heartbeat:
    //   fetch:  read the next opcode byte from memory
    //   decode: execute() figures out what that opcode means
    //   execute: change registers, memory, PC, and/or flags
    // Then we add the instruction's base cycle cost.
    uint8_t opcode = fetch(mem);
    execute(opcode, mem);
    cycles += CYCLE_TABLE[opcode];
}

void CPU::nmi(Memory& mem)
{
    // NMI stands for Non-Maskable Interrupt. "Non-maskable" means the program
    // cannot turn it off with the interrupt-disable flag.
    // Non-maskable interrupt: always accepted and always uses $FFFA/$FFFB.
    serviceInterrupt(mem, 0xFFFA, false);
    cycles += 7;
}

void CPU::irq(Memory& mem)
{
    // IRQ stands for Interrupt Request. Programs can temporarily block IRQs by
    // setting the I flag, which is useful while doing timing-sensitive work.
    // IRQ is maskable. If the I flag is set, the CPU ignores the request.
    if (!getFlag(IRQ_DISABLE)) {
        serviceInterrupt(mem, 0xFFFE, false);
        cycles += 7;
    }
}

bool CPU::getFlag(StatusFlag flag) const
{
    return (P & flag) != 0;
}

void CPU::setFlag(StatusFlag flag, bool value)
{
    if (value) {
        P |= flag;
    } else {
        P &= static_cast<uint8_t>(~flag);
    }
    // Bit 5 is not controlled by instructions. On the real 6502 it behaves as
    // a constant 1 when status is pushed/read, so keep it set after changes.
    P |= UNUSED;
}

void CPU::setZN(uint8_t value)
{
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, (value & 0x80) != 0);
}

uint8_t CPU::fetch(Memory& mem)
{
    // PC always points at "the next byte the CPU will consume."
    // After consuming it, PC moves forward automatically.
    // Post-increment is important: after reading an opcode or operand, PC
    // already points to the next byte the CPU would fetch.
    return mem.read(PC++);
}

uint16_t CPU::fetch16(Memory& mem)
{
    // A 16-bit address needs two 8-bit memory bytes.
    // If memory contains: 00 80
    // the CPU interprets that as address $8000, not $0080.
    // 6502 operands are little-endian: low byte first, high byte second.
    uint8_t lo = fetch(mem);
    uint8_t hi = fetch(mem);
    return static_cast<uint16_t>(lo | (hi << 8));
}

uint16_t CPU::read16(const Memory& mem, uint16_t address) const
{
    uint8_t lo = mem.read(address);
    uint8_t hi = mem.read(static_cast<uint16_t>(address + 1));
    return static_cast<uint16_t>(lo | (hi << 8));
}

uint16_t CPU::read16Bug(const Memory& mem, uint16_t address) const
{
    // This intentionally copies a real hardware mistake. If a JMP indirect
    // pointer is at $12FF, the high byte is read from $1200 instead of $1300.
    // JMP (addr) on a real 6502 has a hardware bug: if addr ends in $FF, the
    // high byte wraps within the same page instead of reading from next page.
    uint8_t lo = mem.read(address);
    uint16_t hiAddress = (address & 0xFF00) | static_cast<uint8_t>(address + 1);
    uint8_t hi = mem.read(hiAddress);
    return static_cast<uint16_t>(lo | (hi << 8));
}

void CPU::push(Memory& mem, uint8_t value)
{
    // Push means "put one byte on top of the stack."
    // The stack grows downward on the 6502, so SP gets smaller after a push.
    // Stack addresses are $0100 | SP. Push writes first, then decrements SP.
    mem.write(static_cast<uint16_t>(0x0100 | SP), value);
    SP--;
}

uint8_t CPU::pop(Memory& mem)
{
    // Pop means "remove and return the most recently pushed byte."
    // Because push decremented SP after writing, pop increments before reading.
    // Pop reverses push: increment SP first, then read from page 1.
    SP++;
    return mem.read(static_cast<uint16_t>(0x0100 | SP));
}

void CPU::push16(Memory& mem, uint16_t value)
{
    // Subroutines and interrupts need to save a full 16-bit return address.
    // Since the stack stores bytes, a 16-bit address is pushed as two bytes.
    // The 6502 pushes 16-bit return addresses high byte first, then low byte.
    push(mem, static_cast<uint8_t>((value >> 8) & 0xFF));
    push(mem, static_cast<uint8_t>(value & 0xFF));
}

uint16_t CPU::pop16(Memory& mem)
{
    uint8_t lo = pop(mem);
    uint8_t hi = pop(mem);
    return static_cast<uint16_t>(lo | (hi << 8));
}

uint16_t CPU::operandAddress(Memory& mem, AddressMode mode, bool* pageCrossed)
{
    // An operand is the value an instruction works with.
    //
    // Some instructions contain the value directly:
    //   LDA #$42   -> operand is the number $42
    //
    // Other instructions contain an address where the value lives:
    //   LDA $2000  -> operand is memory[$2000]
    //
    // This function handles the second kind: it calculates the memory address
    // that should be read from or written to.
    // Addressing modes answer: "Where is the operand?" They do not perform
    // the instruction itself. This lets LDA, ADC, CMP, etc. share the same
    // address calculation code.
    if (pageCrossed) {
        *pageCrossed = false;
    }

    switch (mode) {
    case AddressMode::ZeroPage:
        // "Zero page" means the first 256 bytes of memory: $0000-$00FF.
        // These addresses fit in one byte, so zero-page instructions are short
        // and fast.
        // Zero page uses one operand byte, so the address is always $00xx.
        return fetch(mem);
    case AddressMode::ZeroPageX:
        // Add X to the one-byte zero-page address. Because the address is only
        // 8 bits, $FF + 1 wraps around to $00.
        // Zero-page indexing wraps at $FF just like an 8-bit address bus.
        return static_cast<uint8_t>(fetch(mem) + X);
    case AddressMode::ZeroPageY:
        return static_cast<uint8_t>(fetch(mem) + Y);
    case AddressMode::Absolute:
        // "Absolute" means the instruction contains the full 16-bit address.
        // Example bytes AD 00 20 mean LDA $2000.
        return fetch16(mem);
    case AddressMode::AbsoluteX: {
        // Absolute,X starts with a full 16-bit address and adds X.
        // Example: LDA $2000,X with X=$05 reads from $2005.
        // Absolute indexed modes can cross from one 256-byte page to another.
        // Some instructions charge one extra cycle for that.
        uint16_t base = fetch16(mem);
        uint16_t address = static_cast<uint16_t>(base + X);
        if (pageCrossed) {
            *pageCrossed = (base & 0xFF00) != (address & 0xFF00);
        }
        return address;
    }
    case AddressMode::AbsoluteY: {
        uint16_t base = fetch16(mem);
        uint16_t address = static_cast<uint16_t>(base + Y);
        if (pageCrossed) {
            *pageCrossed = (base & 0xFF00) != (address & 0xFF00);
        }
        return address;
    }
    case AddressMode::Indirect:
        // Indirect means the instruction points to memory that contains the
        // real address. JMP ($3000) reads the jump target from $3000/$3001.
        // Only JMP uses full indirect addressing.
        return read16Bug(mem, fetch16(mem));
    case AddressMode::IndexedIndirect: {
        // Indexed indirect is written like ($20,X).
        // First add X to $20 inside zero page, then read a 16-bit address from
        // that zero-page location. Finally use that address as the operand.
        // (zp,X): add X to the zero-page pointer byte, then read the 16-bit
        // target address from that wrapped zero-page location.
        uint8_t pointer = static_cast<uint8_t>(fetch(mem) + X);
        uint8_t lo = mem.read(pointer);
        uint8_t hi = mem.read(static_cast<uint8_t>(pointer + 1));
        return static_cast<uint16_t>(lo | (hi << 8));
    }
    case AddressMode::IndirectIndexed: {
        // Indirect indexed is written like ($20),Y.
        // First read a 16-bit base address from zero page $20/$21, then add Y.
        // (zp),Y: read a base address from zero page, then add Y to it.
        uint8_t pointer = fetch(mem);
        uint8_t lo = mem.read(pointer);
        uint8_t hi = mem.read(static_cast<uint8_t>(pointer + 1));
        uint16_t base = static_cast<uint16_t>(lo | (hi << 8));
        uint16_t address = static_cast<uint16_t>(base + Y);
        if (pageCrossed) {
            *pageCrossed = (base & 0xFF00) != (address & 0xFF00);
        }
        return address;
    }
    default:
        return 0;
    }
}

uint8_t CPU::readOperand(Memory& mem, AddressMode mode, bool* pageCrossed)
{
    if (mode == AddressMode::Immediate) {
        // Immediate addressing is the simplest kind: the next byte is the
        // actual value, not an address.
        // Immediate operands are literal bytes embedded after the opcode.
        return fetch(mem);
    }

    return mem.read(operandAddress(mem, mode, pageCrossed));
}

void CPU::writeOperand(Memory& mem, AddressMode mode, uint8_t value)
{
    // Store instructions calculate an address, then write a register there.
    mem.write(operandAddress(mem, mode), value);
}

void CPU::serviceInterrupt(Memory& mem, uint16_t vector, bool breakFlag)
{
    // A vector is a fixed memory location that contains an address.
    // Interrupt vectors tell the CPU where to jump when an interrupt happens:
    //   NMI vector: $FFFA/$FFFB
    //   RESET vector: $FFFC/$FFFD
    //   IRQ/BRK vector: $FFFE/$FFFF
    // Interrupts save the current PC and status to the stack, disable further
    // IRQs, then load a new PC from the vector address.
    push16(mem, PC);
    uint8_t status = P | UNUSED;
    if (breakFlag) {
        status |= BREAK;
    } else {
        status &= static_cast<uint8_t>(~BREAK);
    }
    push(mem, status);
    setFlag(IRQ_DISABLE, true);

    // B is only meaningful in the copy pushed to the stack. It is not stored
    // as a persistent hardware latch inside P.
    P &= static_cast<uint8_t>(~BREAK);
    P |= UNUSED;
    PC = read16(mem, vector);
}

void CPU::branch(Memory& mem, bool condition)
{
    // Branches are tiny conditional jumps. Instead of storing a full address,
    // they store a signed offset from -128 to +127 bytes from the current PC.
    // Branch operands are signed 8-bit offsets relative to PC after the
    // operand byte has been fetched.
    int8_t offset = static_cast<int8_t>(fetch(mem));
    if (!condition) {
        return;
    }

    uint16_t oldPC = PC;
    PC = static_cast<uint16_t>(PC + offset);

    // Taken branches cost one extra cycle. Crossing a page costs one more.
    cycles++;
    if ((oldPC & 0xFF00) != (PC & 0xFF00)) {
        cycles++;
    }
}

void CPU::compare(uint8_t reg, uint8_t value)
{
    // Compare answers "is this register less than, equal to, or greater than
    // this value?" using flags:
    //   C set means reg >= value
    //   Z set means reg == value
    //   N reflects bit 7 of reg - value
    // CMP/CPX/CPY behave like subtraction for flags but do not store a result.
    uint8_t result = static_cast<uint8_t>(reg - value);
    setFlag(CARRY, reg >= value);
    setZN(result);
}

void CPU::adc(uint8_t value)
{
    // Flags after addition:
    //   C tells whether the unsigned result was bigger than 255.
    //   V tells whether signed math overflowed, such as 127 + 1 -> -128.
    //   Z/N describe the final 8-bit result in A.
    // ADC adds operand + A + carry-in. Carry reports unsigned overflow;
    // overflow reports signed overflow.
    uint16_t binary = static_cast<uint16_t>(A + value + (getFlag(CARRY) ? 1 : 0));
    setFlag(OVERFLOW, (~(A ^ value) & (A ^ binary) & 0x80) != 0);

    if (getFlag(DECIMAL)) {
        // Decimal mode treats each nibble as a BCD digit. The adjustment below
        // emulates the 6502's packed decimal correction after binary addition.
        uint16_t low = (A & 0x0F) + (value & 0x0F) + (getFlag(CARRY) ? 1 : 0);
        uint16_t high = (A & 0xF0) + (value & 0xF0);
        if (low > 0x09) {
            low += 0x06;
        }
        if (low > 0x0F) {
            high += 0x10;
        }
        if (high > 0x90) {
            high += 0x60;
        }
        setFlag(CARRY, high > 0xFF);
        A = static_cast<uint8_t>((high & 0xF0) | (low & 0x0F));
    } else {
        setFlag(CARRY, binary > 0xFF);
        A = static_cast<uint8_t>(binary);
    }

    setZN(A);
}

void CPU::sbc(uint8_t value)
{
    // Subtraction uses the carry flag backwards from what beginners often
    // expect: C=1 means "no borrow needed"; C=0 means "subtract one extra."
    // SBC subtracts operand and borrow. On the 6502, carry set means "no
    // borrow", which is why carry-in is inverted in the subtraction.
    bool carryIn = getFlag(CARRY);
    uint16_t binary = static_cast<uint16_t>(A - value - (carryIn ? 0 : 1));
    uint8_t result = static_cast<uint8_t>(binary);
    setFlag(OVERFLOW, ((A ^ result) & (A ^ value) & 0x80) != 0);
    setFlag(CARRY, binary < 0x100);

    if (getFlag(DECIMAL)) {
        // BCD subtraction needs a decimal correction just like ADC.
        int low = (A & 0x0F) - (value & 0x0F) - (carryIn ? 0 : 1);
        int high = (A >> 4) - (value >> 4);
        if (low < 0) {
            low -= 6;
            high--;
        }
        if (high < 0) {
            high -= 6;
        }
        A = static_cast<uint8_t>(((high << 4) & 0xF0) | (low & 0x0F));
    } else {
        A = result;
    }

    setZN(A);
}

void CPU::execute(uint8_t opcode, Memory& mem)
{
    // This is the decode/execute stage.
    //
    // The opcode byte has already been fetched. The switch finds the matching
    // instruction behavior. Many case labels share one block because opcodes
    // like LDA immediate, LDA zero page, and LDA absolute all do the same final
    // operation: load a value into A and update flags. Only operand lookup
    // differs, so decodeInfo(opcode).mode supplies the addressing mode.
    // execute() contains the behavior for all official opcodes. Groups of
    // opcodes that only differ by addressing mode share the same code path.
    bool pageCrossed = false;

    switch (opcode) {
    // Add with carry: A = A + operand + carry flag.
    case 0x69: case 0x65: case 0x75: case 0x6D:
    case 0x7D: case 0x79: case 0x61: case 0x71: {
        auto mode = decodeInfo(opcode).mode;
        adc(readOperand(mem, mode, &pageCrossed));
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;
    }

    // Logical AND: each output bit is 1 only if both input bits were 1.
    case 0x29: case 0x25: case 0x35: case 0x2D:
    case 0x3D: case 0x39: case 0x21: case 0x31: {
        auto mode = decodeInfo(opcode).mode;
        A &= readOperand(mem, mode, &pageCrossed);
        setZN(A);
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;
    }

    // Arithmetic shift left. Accumulator mode modifies A directly; memory
    // modes read, modify, then write the addressed byte.
    case 0x0A: {
        setFlag(CARRY, (A & 0x80) != 0);
        A = static_cast<uint8_t>(A << 1);
        setZN(A);
        break;
    }
    case 0x06: case 0x16: case 0x0E: case 0x1E: {
        auto mode = decodeInfo(opcode).mode;
        uint16_t address = operandAddress(mem, mode);
        uint8_t value = mem.read(address);
        setFlag(CARRY, (value & 0x80) != 0);
        value = static_cast<uint8_t>(value << 1);
        mem.write(address, value);
        setZN(value);
        break;
    }

    // Branch instructions. They change PC only if a flag condition is true.
    // branch() handles signed offsets and extra cycles.
    case 0x90: branch(mem, !getFlag(CARRY)); break;
    case 0xB0: branch(mem, getFlag(CARRY)); break;
    case 0xF0: branch(mem, getFlag(ZERO)); break;
    case 0x30: branch(mem, getFlag(NEGATIVE)); break;
    case 0xD0: branch(mem, !getFlag(ZERO)); break;
    case 0x10: branch(mem, !getFlag(NEGATIVE)); break;
    case 0x50: branch(mem, !getFlag(OVERFLOW)); break;
    case 0x70: branch(mem, getFlag(OVERFLOW)); break;

    // BIT copies bits 7 and 6 from memory into N and V, while Z tests A & M.
    case 0x24: case 0x2C: {
        uint8_t value = readOperand(mem, decodeInfo(opcode).mode);
        setFlag(ZERO, (A & value) == 0);
        setFlag(NEGATIVE, (value & 0x80) != 0);
        setFlag(OVERFLOW, (value & 0x40) != 0);
        break;
    }

    case 0x00:
        // BRK behaves like a software interrupt. The opcode has a padding byte,
        // so PC is advanced once more before being pushed.
        PC++;
        serviceInterrupt(mem, 0xFFFE, true);
        if (haltOnBreak) {
            isHalted = true;
        }
        break;

    case 0x18: setFlag(CARRY, false); break;
    case 0xD8: setFlag(DECIMAL, false); break;
    case 0x58: setFlag(IRQ_DISABLE, false); break;
    case 0xB8: setFlag(OVERFLOW, false); break;

    // Compare instructions set flags as if subtraction happened, but leave the
    // compared register unchanged.
    case 0xC9: case 0xC5: case 0xD5: case 0xCD:
    case 0xDD: case 0xD9: case 0xC1: case 0xD1: {
        compare(A, readOperand(mem, decodeInfo(opcode).mode, &pageCrossed));
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;
    }
    case 0xE0: case 0xE4: case 0xEC:
        compare(X, readOperand(mem, decodeInfo(opcode).mode));
        break;
    case 0xC0: case 0xC4: case 0xCC:
        compare(Y, readOperand(mem, decodeInfo(opcode).mode));
        break;

    // Decrement memory/registers.
    case 0xC6: case 0xD6: case 0xCE: case 0xDE: {
        uint16_t address = operandAddress(mem, decodeInfo(opcode).mode);
        uint8_t value = static_cast<uint8_t>(mem.read(address) - 1);
        mem.write(address, value);
        setZN(value);
        break;
    }
    case 0xCA: X--; setZN(X); break;
    case 0x88: Y--; setZN(Y); break;

    // Exclusive OR.
    case 0x49: case 0x45: case 0x55: case 0x4D:
    case 0x5D: case 0x59: case 0x41: case 0x51: {
        A ^= readOperand(mem, decodeInfo(opcode).mode, &pageCrossed);
        setZN(A);
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;
    }

    // Increment memory/registers.
    case 0xE6: case 0xF6: case 0xEE: case 0xFE: {
        uint16_t address = operandAddress(mem, decodeInfo(opcode).mode);
        uint8_t value = static_cast<uint8_t>(mem.read(address) + 1);
        mem.write(address, value);
        setZN(value);
        break;
    }
    case 0xE8: X++; setZN(X); break;
    case 0xC8: Y++; setZN(Y); break;

    // Jumps and subroutines. JSR pushes PC - 1; RTS pulls it and adds 1.
    case 0x4C: PC = operandAddress(mem, AddressMode::Absolute); break;
    case 0x6C: PC = operandAddress(mem, AddressMode::Indirect); break;
    case 0x20: {
        uint16_t target = fetch16(mem);
        push16(mem, static_cast<uint16_t>(PC - 1));
        PC = target;
        break;
    }

    // Load instructions.
    case 0xA9: case 0xA5: case 0xB5: case 0xAD:
    case 0xBD: case 0xB9: case 0xA1: case 0xB1:
        A = readOperand(mem, decodeInfo(opcode).mode, &pageCrossed);
        setZN(A);
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;

    case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
        X = readOperand(mem, decodeInfo(opcode).mode, &pageCrossed);
        setZN(X);
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;

    case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
        Y = readOperand(mem, decodeInfo(opcode).mode, &pageCrossed);
        setZN(Y);
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;

    // Logical shift right.
    case 0x4A:
        setFlag(CARRY, (A & 0x01) != 0);
        A >>= 1;
        setZN(A);
        break;
    case 0x46: case 0x56: case 0x4E: case 0x5E: {
        uint16_t address = operandAddress(mem, decodeInfo(opcode).mode);
        uint8_t value = mem.read(address);
        setFlag(CARRY, (value & 0x01) != 0);
        value >>= 1;
        mem.write(address, value);
        setZN(value);
        break;
    }

    case 0xEA: break;

    // Logical OR.
    case 0x09: case 0x05: case 0x15: case 0x0D:
    case 0x1D: case 0x19: case 0x01: case 0x11:
        A |= readOperand(mem, decodeInfo(opcode).mode, &pageCrossed);
        setZN(A);
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;

    // Stack instructions. PHA/PHP push values onto the stack; PLA/PLP pull
    // the most recent values back.
    case 0x48: push(mem, A); break;
    case 0x08: push(mem, P | BREAK | UNUSED); break;
    case 0x68: A = pop(mem); setZN(A); break;
    case 0x28: P = (pop(mem) | UNUSED) & static_cast<uint8_t>(~BREAK); break;

    // Rotate left through carry.
    case 0x2A: {
        bool oldCarry = getFlag(CARRY);
        setFlag(CARRY, (A & 0x80) != 0);
        A = static_cast<uint8_t>((A << 1) | (oldCarry ? 1 : 0));
        setZN(A);
        break;
    }
    case 0x26: case 0x36: case 0x2E: case 0x3E: {
        uint16_t address = operandAddress(mem, decodeInfo(opcode).mode);
        uint8_t value = mem.read(address);
        bool oldCarry = getFlag(CARRY);
        setFlag(CARRY, (value & 0x80) != 0);
        value = static_cast<uint8_t>((value << 1) | (oldCarry ? 1 : 0));
        mem.write(address, value);
        setZN(value);
        break;
    }

    // Rotate right through carry.
    case 0x6A: {
        bool oldCarry = getFlag(CARRY);
        setFlag(CARRY, (A & 0x01) != 0);
        A = static_cast<uint8_t>((A >> 1) | (oldCarry ? 0x80 : 0));
        setZN(A);
        break;
    }
    case 0x66: case 0x76: case 0x6E: case 0x7E: {
        uint16_t address = operandAddress(mem, decodeInfo(opcode).mode);
        uint8_t value = mem.read(address);
        bool oldCarry = getFlag(CARRY);
        setFlag(CARRY, (value & 0x01) != 0);
        value = static_cast<uint8_t>((value >> 1) | (oldCarry ? 0x80 : 0));
        mem.write(address, value);
        setZN(value);
        break;
    }

    // Return from interrupt / subroutine. These restore PC so execution
    // continues where the CPU left off.
    case 0x40:
        P = (pop(mem) | UNUSED) & static_cast<uint8_t>(~BREAK);
        PC = pop16(mem);
        break;
    case 0x60:
        PC = static_cast<uint16_t>(pop16(mem) + 1);
        break;

    // Subtract with borrow.
    case 0xE9: case 0xE5: case 0xF5: case 0xED:
    case 0xFD: case 0xF9: case 0xE1: case 0xF1:
        sbc(readOperand(mem, decodeInfo(opcode).mode, &pageCrossed));
        if (pageCrossed && addsPageCycle(opcode)) cycles++;
        break;

    // Set flag instructions.
    case 0x38: setFlag(CARRY, true); break;
    case 0xF8: setFlag(DECIMAL, true); break;
    case 0x78: setFlag(IRQ_DISABLE, true); break;

    // Store instructions.
    case 0x85: case 0x95: case 0x8D: case 0x9D:
    case 0x99: case 0x81: case 0x91:
        writeOperand(mem, decodeInfo(opcode).mode, A);
        break;
    case 0x86: case 0x96: case 0x8E:
        writeOperand(mem, decodeInfo(opcode).mode, X);
        break;
    case 0x84: case 0x94: case 0x8C:
        writeOperand(mem, decodeInfo(opcode).mode, Y);
        break;

    // Register transfers.
    case 0xAA: X = A; setZN(X); break;
    case 0xA8: Y = A; setZN(Y); break;
    case 0xBA: X = SP; setZN(X); break;
    case 0x8A: A = X; setZN(A); break;
    case 0x9A: SP = X; break;
    case 0x98: A = Y; setZN(A); break;

    default:
        if (enableIllegalOpcodes && executeIllegalOpcode(opcode, mem)) {
            break;
        }

        std::cout << "Unknown opcode: 0x" << hexByte(opcode) << "\n";
        isHalted = true;
        break;
    }
}

bool CPU::executeIllegalOpcode(uint8_t opcode, Memory& mem)
{
    // These are common undocumented NMOS 6502 opcodes. They are useful for
    // compatibility experiments, but they stay optional because real official
    // 6502 code should never depend on them.
    bool pageCrossed = false;
    auto mode = decodeInfo(opcode).mode;

    auto readModifyWrite = [&](auto operation) {
        // Many illegal opcodes combine a memory modification with an A-register
        // operation. This helper performs the shared read -> change -> write
        // part and returns the new memory value for the second half.
        uint16_t address = operandAddress(mem, mode);
        uint8_t value = mem.read(address);
        value = operation(value);
        mem.write(address, value);
        return value;
    };

    switch (opcode) {
    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
        // Single-byte unofficial NOPs: consume time, change nothing.
        cycles += illegalOpcodeCycles(opcode);
        return true;

    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
    case 0x04: case 0x44: case 0x64:
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
    case 0x0C:
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        readOperand(mem, mode, &pageCrossed);
        cycles += illegalOpcodeCycles(opcode);
        if (pageCrossed && illegalNopAddsPageCycle(opcode)) {
            cycles++;
        }
        return true;

    case 0x0B: case 0x2B:
        // ANC*: AND immediate with A, then copy bit 7 of A into carry.
        A &= readOperand(mem, AddressMode::Immediate);
        setZN(A);
        setFlag(CARRY, getFlag(NEGATIVE));
        cycles += illegalOpcodeCycles(opcode);
        return true;

    case 0x4B:
        // ALR*: AND immediate with A, then logical shift right.
        A &= readOperand(mem, AddressMode::Immediate);
        setFlag(CARRY, (A & 0x01) != 0);
        A >>= 1;
        setZN(A);
        cycles += illegalOpcodeCycles(opcode);
        return true;

    case 0x6B: {
        // ARR*: AND immediate, rotate right through carry, then set C/V from
        // bits 6 and 5. Software rarely uses this outside compatibility tests.
        A &= readOperand(mem, AddressMode::Immediate);
        A = static_cast<uint8_t>((A >> 1) | (getFlag(CARRY) ? 0x80 : 0));
        setZN(A);
        setFlag(CARRY, (A & 0x40) != 0);
        setFlag(OVERFLOW, ((A >> 6) ^ (A >> 5)) & 0x01);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0xCB: {
        // AXS*: compare (A & X) with immediate and store the subtraction in X.
        uint8_t value = readOperand(mem, AddressMode::Immediate);
        uint8_t source = static_cast<uint8_t>(A & X);
        uint8_t result = static_cast<uint8_t>(source - value);
        setFlag(CARRY, source >= value);
        X = result;
        setZN(X);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0xEB:
        // SBC* is an unofficial duplicate of SBC immediate.
        sbc(readOperand(mem, AddressMode::Immediate));
        cycles += illegalOpcodeCycles(opcode);
        return true;

    case 0xA3: case 0xA7: case 0xAF: case 0xB3: case 0xB7: case 0xBF:
        // LAX*: load the same memory value into both A and X.
        A = readOperand(mem, mode, &pageCrossed);
        X = A;
        setZN(A);
        cycles += illegalOpcodeCycles(opcode);
        if (pageCrossed && (opcode == 0xB3 || opcode == 0xBF)) {
            cycles++;
        }
        return true;

    case 0x83: case 0x87: case 0x8F: case 0x97:
        // SAX*: store A & X to memory. Neither A nor X changes.
        writeOperand(mem, mode, static_cast<uint8_t>(A & X));
        cycles += illegalOpcodeCycles(opcode);
        return true;

    case 0x03: case 0x07: case 0x0F: case 0x13: case 0x17: case 0x1B: case 0x1F: {
        // SLO*: ASL memory, then ORA the shifted value into A.
        uint8_t value = readModifyWrite([&](uint8_t original) {
            setFlag(CARRY, (original & 0x80) != 0);
            return static_cast<uint8_t>(original << 1);
        });
        A |= value;
        setZN(A);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0x23: case 0x27: case 0x2F: case 0x33: case 0x37: case 0x3B: case 0x3F: {
        // RLA*: ROL memory, then AND the rotated value with A.
        uint8_t value = readModifyWrite([&](uint8_t original) {
            bool oldCarry = getFlag(CARRY);
            setFlag(CARRY, (original & 0x80) != 0);
            return static_cast<uint8_t>((original << 1) | (oldCarry ? 1 : 0));
        });
        A &= value;
        setZN(A);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0x43: case 0x47: case 0x4F: case 0x53: case 0x57: case 0x5B: case 0x5F: {
        // SRE*: LSR memory, then EOR the shifted value with A.
        uint8_t value = readModifyWrite([&](uint8_t original) {
            setFlag(CARRY, (original & 0x01) != 0);
            return static_cast<uint8_t>(original >> 1);
        });
        A ^= value;
        setZN(A);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0x63: case 0x67: case 0x6F: case 0x73: case 0x77: case 0x7B: case 0x7F: {
        // RRA*: ROR memory, then ADC the rotated value into A.
        uint8_t value = readModifyWrite([&](uint8_t original) {
            bool oldCarry = getFlag(CARRY);
            setFlag(CARRY, (original & 0x01) != 0);
            return static_cast<uint8_t>((original >> 1) | (oldCarry ? 0x80 : 0));
        });
        adc(value);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0xC3: case 0xC7: case 0xCF: case 0xD3: case 0xD7: case 0xDB: case 0xDF: {
        // DCP*: DEC memory, then CMP the decremented value with A.
        uint8_t value = readModifyWrite([](uint8_t original) {
            return static_cast<uint8_t>(original - 1);
        });
        compare(A, value);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0xE3: case 0xE7: case 0xEF: case 0xF3: case 0xF7: case 0xFB: case 0xFF: {
        // ISB/ISC*: INC memory, then SBC the incremented value from A.
        uint8_t value = readModifyWrite([](uint8_t original) {
            return static_cast<uint8_t>(original + 1);
        });
        sbc(value);
        cycles += illegalOpcodeCycles(opcode);
        return true;
    }

    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
    case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
        // KIL/JAM opcodes lock the CPU on real hardware. In the emulator,
        // halting is the closest useful behavior.
        std::cout << "[KIL] CPU locked by illegal halt opcode.\n";
        isHalted = true;
        return true;
    }

    return false;
}

std::string CPU::disassemble(const Memory& mem, uint16_t address, uint8_t* bytesUsed) const
{
    // Disassembly is a read-only view of memory. It decodes the opcode at the
    // requested address and formats the operand according to its addressing mode.
    uint8_t opcode = mem.read(address);
    InstructionInfo info = decodeInfo(opcode);
    if (bytesUsed) {
        *bytesUsed = info.bytes;
    }

    uint8_t operand8 = mem.read(static_cast<uint16_t>(address + 1));
    uint16_t operand16 = static_cast<uint16_t>(operand8 | (mem.read(static_cast<uint16_t>(address + 2)) << 8));

    std::ostringstream out;

    // First print address and raw instruction bytes, matching the style used
    // by many 6502 debuggers and test traces.
    out << hexWord(address) << "  ";
    for (uint8_t i = 0; i < 3; i++) {
        if (i < info.bytes) {
            out << hexByte(mem.read(static_cast<uint16_t>(address + i))) << ' ';
        } else {
            out << "   ";
        }
    }

    out << ' ' << info.mnemonic;

    // Then print the operand in assembly syntax.
    switch (info.mode) {
    case AddressMode::Implied:
        break;
    case AddressMode::Accumulator:
        out << " A";
        break;
    case AddressMode::Immediate:
        out << " #$" << hexByte(operand8);
        break;
    case AddressMode::ZeroPage:
        out << " $" << hexByte(operand8);
        break;
    case AddressMode::ZeroPageX:
        out << " $" << hexByte(operand8) << ",X";
        break;
    case AddressMode::ZeroPageY:
        out << " $" << hexByte(operand8) << ",Y";
        break;
    case AddressMode::Relative: {
        uint16_t target = static_cast<uint16_t>(address + info.bytes + static_cast<int8_t>(operand8));
        out << " $" << hexWord(target);
        break;
    }
    case AddressMode::Absolute:
        out << " $" << hexWord(operand16);
        break;
    case AddressMode::AbsoluteX:
        out << " $" << hexWord(operand16) << ",X";
        break;
    case AddressMode::AbsoluteY:
        out << " $" << hexWord(operand16) << ",Y";
        break;
    case AddressMode::Indirect:
        out << " ($" << hexWord(operand16) << ")";
        break;
    case AddressMode::IndexedIndirect:
        out << " ($" << hexByte(operand8) << ",X)";
        break;
    case AddressMode::IndirectIndexed:
        out << " ($" << hexByte(operand8) << "),Y";
        break;
    }

    return out.str();
}

std::string CPU::registers() const
{
    // Compact debugger-friendly register dump.
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0')
        << "A=$" << std::setw(2) << static_cast<int>(A)
        << " X=$" << std::setw(2) << static_cast<int>(X)
        << " Y=$" << std::setw(2) << static_cast<int>(Y)
        << " SP=$" << std::setw(2) << static_cast<int>(SP)
        << " PC=$" << std::setw(4) << PC
        << " P=$" << std::setw(2) << static_cast<int>(P)
        << std::dec << " CYC=" << cycles;
    return out.str();
}
