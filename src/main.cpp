#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpu.h"
#include "memory.h"

// main.cpp is not the CPU itself. It is a small front panel around the CPU:
// it creates memory, loads a program into memory, sets the reset vector, then
// repeatedly asks CPU::step() to execute one instruction at a time.
namespace {
uint16_t parseAddress(const std::string& text)
{
    // Accept decimal, $C000, or 0xC000 so debugger commands feel natural.
    std::size_t start = 0;
    int base = 10;

    if (!text.empty() && text[0] == '$') {
        start = 1;
        base = 16;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        start = 2;
        base = 16;
    }

    return static_cast<uint16_t>(std::stoul(text.substr(start), nullptr, base));
}

void loadProgram(Memory& mem, uint16_t start, const std::vector<uint8_t>& program)
{
    // A CPU only knows about bytes in memory. Loading a program means copying
    // the program's machine-code bytes into memory at the address where we
    // want execution to begin.
    // Raw assembled binaries do not contain address metadata, so the caller
    // decides where the first byte should be placed in the 64KB address space.
    for (std::size_t i = 0; i < program.size(); i++) {
        mem.write(static_cast<uint16_t>(start + i), program[i]);
    }
}

std::vector<uint8_t> loadBinaryFile(const std::string& path)
{
    // Read the file exactly as bytes. No parsing, headers, or relocation.
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open binary file: " + path);
    }

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

void writeVector(Memory& mem, uint16_t vector, uint16_t address)
{
    // A vector is like a tiny pointer stored at a special CPU-known address.
    // RESET reads $FFFC/$FFFD, so writing the reset vector tells the CPU where
    // the program starts after cpu.reset().
    // 6502 vectors are little-endian addresses stored at fixed memory slots.
    mem.write(vector, static_cast<uint8_t>(address & 0xFF));
    mem.write(static_cast<uint16_t>(vector + 1), static_cast<uint8_t>((address >> 8) & 0xFF));
}

void printUsage(const char* exe)
{
    std::cout
        << "Usage:\n"
        << "  " << exe << "\n"
        << "  " << exe << " program.bin [--load 0x0600] [--pc 0x0600] [--debug] [--max 1000000]\n\n"
        << "Debugger commands: s/step, r/run [count], regs, d/disasm [addr] [count],\n"
        << "                   m/mem addr [len], irq, nmi, q/quit\n";
}

void dumpMemory(const Memory& mem, uint16_t address, uint16_t length)
{
    // Hex dump memory in 16-byte rows, which makes zero page and stack pages
    // easy to inspect while stepping.
    for (uint16_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            std::cout << "\n$" << std::uppercase << std::hex << std::setfill('0')
                      << std::setw(4) << static_cast<uint16_t>(address + i) << ": ";
        }

        std::cout << std::setw(2) << static_cast<int>(mem.read(static_cast<uint16_t>(address + i))) << ' ';
    }
    std::cout << std::dec << "\n";
}

void disassembleRange(const CPU& cpu, const Memory& mem, uint16_t address, int count)
{
    // Disassembly converts machine-code bytes back into readable assembly.
    // It is for humans only; it does not change CPU state or execute anything.
    // Each instruction can be 1, 2, or 3 bytes, so ask disassemble() how far
    // to advance after every line.
    uint16_t pc = address;
    for (int i = 0; i < count; i++) {
        uint8_t bytes = 1;
        std::cout << cpu.disassemble(mem, pc, &bytes) << "\n";
        pc = static_cast<uint16_t>(pc + bytes);
    }
}

void runProgram(CPU& cpu, Memory& mem, uint64_t maxSteps, bool trace)
{
    // Running a program is just stepping repeatedly.
    // With tracing enabled, print the instruction before it runs and registers
    // after it runs, so you can watch state change instruction by instruction.
    // Non-interactive run loop. The step limit protects you from programs that
    // intentionally loop forever or jump into empty memory.
    uint64_t steps = 0;
    while (!cpu.isHalted && steps < maxSteps) {
        if (trace) {
            std::cout << cpu.disassemble(mem, cpu.PC) << "    ";
        }

        cpu.step(mem);

        if (trace) {
            std::cout << cpu.registers() << "\n";
        }
        steps++;
    }

    if (steps == maxSteps && !cpu.isHalted) {
        std::cout << "Stopped after max step count (" << maxSteps << ").\n";
    }
}

void debugger(CPU& cpu, Memory& mem)
{
    // This debugger is intentionally simple: it pauses between instructions
    // and lets you decide what to inspect or whether to continue running.
    // Very small monitor/debugger: show current instruction + registers, read
    // one command, then either step, run, inspect, or trigger an interrupt.
    std::string line;
    while (!cpu.isHalted) {
        std::cout << cpu.disassemble(mem, cpu.PC) << "\n";
        std::cout << cpu.registers() << "\n";
        std::cout << "dbg> ";

        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command.empty() || command == "s" || command == "step") {
            // Step means execute exactly one instruction.
            cpu.step(mem);
        } else if (command == "r" || command == "run") {
            // Run means keep stepping until BRK/halt or a maximum instruction count.
            uint64_t count = 1000000;
            input >> count;
            runProgram(cpu, mem, count, false);
        } else if (command == "regs") {
            std::cout << cpu.registers() << "\n";
        } else if (command == "d" || command == "disasm") {
            std::string addressText;
            int count = 10;
            input >> addressText >> count;
            uint16_t address = addressText.empty() ? cpu.PC : parseAddress(addressText);
            disassembleRange(cpu, mem, address, count);
        } else if (command == "m" || command == "mem") {
            std::string addressText;
            uint16_t length = 64;
            input >> addressText >> length;
            if (addressText.empty()) {
                std::cout << "Memory command needs an address.\n";
            } else {
                dumpMemory(mem, parseAddress(addressText), length);
            }
        } else if (command == "irq") {
            // Manually trigger interrupt behavior so it can be tested.
            cpu.irq(mem);
        } else if (command == "nmi") {
            cpu.nmi(mem);
        } else if (command == "q" || command == "quit") {
            break;
        } else if (command == "h" || command == "help") {
            printUsage("emulator");
        } else {
            std::cout << "Unknown debugger command. Type help for commands.\n";
        }
    }
}
}

int main(int argc, char* argv[])
{
    try {
        Memory mem;
        CPU cpu;
        mem.reset();

        // Default binary load location. $0600 is a common address for simple
        // 6502 monitor examples, while the built-in sample below uses $0200.
        uint16_t loadAddress = 0x0600;
        uint16_t pcAddress = loadAddress;
        bool pcProvided = false;
        bool debug = false;
        bool trace = true;
        uint64_t maxSteps = 1000000;
        std::string binaryPath;

        for (int i = 1; i < argc; i++) {
            // Minimal argument parser. Options that need a value consume the
            // following argv entry.
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            } else if (arg == "--load" && i + 1 < argc) {
                loadAddress = parseAddress(argv[++i]);
                if (!pcProvided) {
                    pcAddress = loadAddress;
                }
            } else if (arg == "--pc" && i + 1 < argc) {
                pcAddress = parseAddress(argv[++i]);
                pcProvided = true;
            } else if (arg == "--debug") {
                debug = true;
                trace = false;
            } else if (arg == "--quiet") {
                trace = false;
            } else if (arg == "--no-break-halt") {
                cpu.haltOnBreak = false;
            } else if (arg == "--max" && i + 1 < argc) {
                maxSteps = std::stoull(argv[++i]);
            } else if (binaryPath.empty()) {
                binaryPath = arg;
            } else {
                throw std::runtime_error("Unexpected argument: " + arg);
            }
        }

        if (binaryPath.empty()) {
            // No file given: run a tiny built-in program so the emulator can be
            // tested immediately after building.
            loadAddress = 0x0200;
            pcAddress = loadAddress;
            loadProgram(mem, loadAddress, {
                0xA2, 0x01,       // LDX #$01
                0xA9, 0x42,       // LDA #$42
                0x95, 0x10,       // STA $10,X
                0xA9, 0x00,       // LDA #$00
                0xB5, 0x10,       // LDA $10,X
                0xAA,             // TAX
                0xE8,             // INX
                0x00              // BRK
            });
        } else {
            // File given: load bytes exactly where requested.
            std::vector<uint8_t> program = loadBinaryFile(binaryPath);
            loadProgram(mem, loadAddress, program);
            std::cout << "Loaded " << program.size() << " bytes at $"
                      << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                      << loadAddress << std::dec << "\n";
        }

        // Set reset/NMI/IRQ vectors after loading memory, then reset the CPU so
        // PC is read from the reset vector instead of being assigned directly.
        writeVector(mem, 0xFFFC, pcAddress);
        writeVector(mem, 0xFFFA, 0x0000);
        writeVector(mem, 0xFFFE, 0x0000);
        cpu.reset(mem);

        if (debug) {
            debugger(cpu, mem);
        } else {
            runProgram(cpu, mem, maxSteps, trace);
        }

        std::cout << cpu.registers() << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
