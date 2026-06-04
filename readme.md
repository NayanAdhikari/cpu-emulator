CPU Emulator

A low-level CPU emulator written in C++ that models the core architecture of a 6502-inspired 8-bit processor. Built from scratch as a personal project to deepen my understanding of computer architecture and help me study for good eats.

---

What It Does

This emulator models the fundamental **fetch → decode → execute** cycle of a real CPU, including:

- A **64KB memory model** with read/write access
- Accurate **CPU registers** — Accumulator (A), Index (X, Y), Stack Pointer (SP), and Program Counter (PC)
- **Status flags** — Zero, Negative, Carry, Overflow, and more
- A working **opcode decoder** with implemented 6502 instructions

---

Implemented Opcodes

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| `0xA9` | LDA Immediate | Load a value into the Accumulator |
| `0xA5` | LDA Zero Page | Load Accumulator from memory |
| `0xAA` | TAX | Transfer Accumulator to X |
| `0x85` | STA | Store Accumulator into memory |
| `0x69` | ADC | Add with Carry |
| `0xE8` | INX | Increment X |
| `0xC8` | INY | Increment Y |
| `0xCA` | DEX | Decrement X |
| `0x88` | DEY | Decrement Y |
| `0x00` | BRK | Break / halt execution |

---

Project Structure

```
CPUEmulator/
├── src/
│   ├── main.cpp       # Entry point, loads and runs a test program
│   ├── cpu.h / cpu.cpp        # CPU registers, flags, and execution loop
│   └── memory.h / memory.cpp  # 64KB memory model
├── CMakeLists.txt
└── tests/
```

---

How to Build & Run

**Requirements:** g++ with C++17 support and i hope gdb is working on your computer

```bash
git clone https://github.com/NayanAdhikari/cpu-emulator.git
cd cpu-emulator
g++ src/main.cpp src/cpu.cpp src/memory.cpp -o emulator -Wall -Wextra -std=c++17
./emulator.exe   # Windows
./emulator       # Mac/Linux
```

Expected output:
```
A=5 X=0 PC=202
A=5 X=5 PC=203
A=5 X=4 PC=204
A=5 X=4 PC=205A=5 X=4 PC=206
```

---

What I Learned

- How a CPU's fetch → decode → execute cycle works at the hardware level
- Modeling memory and registers using fixed-width integer types (`uint8_t`, `uint16_t`)
- How opcodes map to real CPU instructions
- C++ concepts including structs, references, bit-fields, and header files
- Using Git and GitHub for version control

---

Roadmap

- [ ] Implement the full 6502 instruction set (~56 opcodes)
- [ ] Add all 13 addressing modes
- [ ] Implement interrupt handling (NMI, IRQ, RESET)
- [ ] Load and execute real assembled programs from binary files
- [ ] Add a basic debugger/disassembler view

---

About

Built by **Nayan Adhikari**, a rising junior and incoming ECE student at PHSC. This project is part of my self-directed study in computer architecture.
