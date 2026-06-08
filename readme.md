CPU Emulator

A low-level CPU emulator written in C++ that models the core architecture of a
6502-inspired 8-bit processor. Built from scratch as a personal project to
deepen my understanding of computer architecture.

What It Does

This emulator models the fundamental fetch → decode → execute cycle of a real
CPU, including:

- A 64KB memory model with read/write access
- Accurate CPU registers — Accumulator (A), Index (X, Y), Stack Pointer (SP),
  and Program Counter (PC)
- A proper 8-bit processor status register (P) with correct bit layout
- A working opcode decoder with implemented 6502 instructions
- Accurate per-instruction cycle counts
- RESET vector support — PC is loaded from $FFFC/$FFFD on startup, matching
  real 6502 hardware behaviour

Implemented Opcodes

| Opcode | Mnemonic          | Description                          |
|--------|-------------------|--------------------------------------|
| 0xA9   | LDA Immediate     | Load value into Accumulator          |
| 0xA5   | LDA Zero Page     | Load Accumulator from zero page      |
| 0xAD   | LDA Absolute      | Load Accumulator from full address   |
| 0x85   | STA Zero Page     | Store Accumulator to zero page       |
| 0x8D   | STA Absolute      | Store Accumulator to full address    |
| 0xAA   | TAX               | Transfer Accumulator to X            |
| 0x69   | ADC Immediate     | Add with Carry                       |
| 0xE8   | INX               | Increment X                          |
| 0xC8   | INY               | Increment Y                          |
| 0xCA   | DEX               | Decrement X                          |
| 0x88   | DEY               | Decrement Y                          |
| 0x48   | PHA               | Push Accumulator to stack            |
| 0x68   | PLA               | Pull Accumulator from stack          |
| 0x08   | PHP               | Push Processor Status to stack       |
| 0x28   | PLP               | Pull Processor Status from stack     |
| 0x20   | JSR               | Jump to Subroutine                   |
| 0x60   | RTS               | Return from Subroutine               |
| 0x4C   | JMP Absolute      | Jump to address                      |
| 0x00   | BRK               | Push state, jump through IRQ vector  |

Project Structure

```
CPUEmulator/
├── src/
│   ├── main.cpp               # Entry point, loads and runs a test program
│   ├── cpu.h / cpu.cpp        # CPU registers, flags, and execution loop
│   └── memory.h / memory.cpp  # 64KB memory model
├── CMakeLists.txt
└── tests/
```
How to Build & Run

Requirements: g++ with C++17 support

```bash
git clone https://github.com/NayanAdhikari/cpu-emulator.git
cd cpu-emulator
g++ src/main.cpp src/cpu.cpp src/memory.cpp -o emulator -Wall -Wextra -std=c++17

./emulator.exe   # Windows
./emulator       # Mac/Linux
```

Expected output:
A=0x42 X=0x0 Y=0x0 SP=0xfe PC=0x204 P=0xa0 CYC=3
A=0x42 X=0x0 Y=0x0 SP=0xfd PC=0x205 P=0xa0 CYC=6
A=0x0  X=0x0 Y=0x0 SP=0xfd PC=0x207 P=0xa2 CYC=8
A=0x42 X=0x0 Y=0x0 SP=0xfe PC=0x208 P=0xa0 CYC=12
[BRK] Pushed state, jumped to IRQ vector.

What I Learned

- How a CPU's fetch → decode → execute cycle works at the hardware level
- Modeling memory and registers using fixed-width integer types (`uint8_t`, `uint16_t`)
- How the 6502 processor status register works as a real 8-bit register with
  defined bit positions, rather than a C++ bitfield
- How the stack works in hardware — descending, page-fixed at $0100–$01FF
- The JSR/RTS return address convention — the 6502 pushes PC-1 and RTS adds 1 back
- How BRK works — pushes PC and status to the stack, jumps through the IRQ vector
- How hardware startup works — reading the RESET vector instead of hardcoding an address
- Accurate cycle counting using a per-opcode lookup table
- C++ concepts including structs, references, and header files
- Using Git and GitHub for version control

Roadmap

- [x] Core fetch → decode → execute loop
- [x] Processor status register (P) with correct bit layout
- [x] Stack operations (PHA, PLA, PHP, PLP)
- [x] Subroutine support (JSR, RTS)
- [x] Jump instructions (JMP)
- [x] Accurate per-instruction cycle counts
- [x] RESET vector support
- [ ] Implement the full 6502 instruction set (~56 opcodes)
- [ ] Add all 13 addressing modes
- [ ] Implement interrupt handling (NMI, IRQ)
- [ ] Load and execute real assembled programs from binary files
- [ ] Add a basic debugger / disassembler view

## About

Built by Nayan Adhikari, a rising junior and incoming ECE student at PHSC.
This project is part of my self-directed study in computer architecture.
