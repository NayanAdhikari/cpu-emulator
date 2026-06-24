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
- NMI and IRQ interrupt entry through the standard 6502 vectors
- Binary program loading for raw assembled 6502 programs
- A basic debugger with registers, memory dump, stepping, run, IRQ/NMI trigger,
  and disassembly
- Memory-mapped console I/O hooks for simple program output/input
- Optional common undocumented opcode support, enabled only when requested
- Automated CPU regression tests for core instructions, stack behavior, I/O,
  and illegal opcode toggling

Implemented CPU Coverage

- Full official 6502 instruction set: 56 documented mnemonics / 151 legal opcodes
- All 13 official addressing modes:
  - implied
  - accumulator
  - immediate
  - zero page
  - zero page,X
  - zero page,Y
  - relative
  - absolute
  - absolute,X
  - absolute,Y
  - indirect
  - indexed indirect, `(addr,X)`
  - indirect indexed, `(addr),Y`
- Correct stack layout at `$0100`–`$01FF`
- RESET, NMI, IRQ/BRK vectors at `$FFFC`, `$FFFA`, and `$FFFE`
- 6502 JMP indirect page-wrap bug emulation
- Base instruction cycles plus page-cross and branch-taken cycle penalties
- Common unofficial NMOS 6502 opcodes can be enabled with `--illegal`

Memory-Mapped I/O

When console I/O is enabled with `--io`, these addresses behave like tiny
devices:

| Address | Behavior |
|---------|----------|
| `$F001` | Store a byte here to print it as a character |
| `$F004` | Read `1` if queued input is available, otherwise `0` |
| `$F005` | Read and consume one queued input byte |

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
g++ tests/cpu_tests.cpp src/cpu.cpp src/memory.cpp -o cpu_tests -Wall -Wextra -std=c++17

./emulator.exe   # Windows
./emulator       # Mac/Linux
./cpu_tests.exe  # Windows tests
```

Optional CMake build:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Run the built-in sample:

```bash
./emulator
```

Load a raw binary at `$0600` and point the RESET vector there:

```bash
./emulator program.bin --load 0x0600
```

Run with memory-mapped console I/O:

```bash
./emulator program.bin --load 0x0600 --io
```

Queue input bytes for `$F005`:

```bash
./emulator program.bin --load 0x0600 --io --input HELLO
```

Enable common undocumented opcodes:

```bash
./emulator program.bin --load 0x0600 --illegal
```

Start the debugger:

```bash
./emulator program.bin --load 0x0600 --debug
```

Debugger commands:

```text
s / step              Step one instruction
r / run [count]       Run until BRK/halt or count instructions
regs                  Print registers
d / disasm [addr] [n] Disassemble n instructions
m / mem addr [len]    Dump memory
irq                   Trigger IRQ if interrupts are enabled
nmi                   Trigger NMI
q / quit              Exit debugger
```

`BRK` pushes PC/status and loads the IRQ vector. The runner halts on BRK by
default so test programs stop cleanly; pass `--no-break-halt` to continue after
the vector jump.

Run automated tests:

```bash
./cpu_tests
```

Expected result:

```text
All CPU emulator tests passed.
```

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
- How addressing modes can be shared across many opcodes without duplicating
  instruction logic
- How a debugger/disassembler can use the same opcode metadata as the CPU core
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
- [x] Implement the full official 6502 instruction set (~56 mnemonics)
- [x] Add all 13 addressing modes
- [x] Implement interrupt handling (NMI, IRQ)
- [x] Load and execute real assembled programs from binary files
- [x] Add a basic debugger / disassembler view
- [x] Add automated opcode regression tests
- [x] Add memory-mapped I/O hooks
- [x] Add optional illegal opcode support
- [ ] Add external test ROM / nestest trace comparison
- [ ] Add a small demo assembly program that prints through `$F001`
- [ ] Add breakpoints and watchpoints to the debugger

## About

Built by Nayan Adhikari, a rising junior and incoming ECE student at PHSC.
This project is part of my self-directed study in computer architecture.
