#include "memory.h"
#include <ostream>

void Memory::reset() { data.fill(0); }

void Memory::enableConsoleIO(std::ostream& out) {
    // This does not change the CPU at all. It only tells Memory to treat a few
    // addresses as device registers instead of plain RAM.
    ioEnabled = true;
    output = &out;
}

void Memory::queueInput(uint8_t value) {
    // Store one future input byte. The oldest queued byte is read first, just
    // like a simple keyboard/input buffer.
    inputBuffer.push_back(value);
}

void Memory::queueInput(const char* text) {
    // Convenience helper for command-line input like --input HELLO.
    while (*text != '\0') {
        queueInput(static_cast<uint8_t>(*text));
        text++;
    }
}

uint8_t Memory::read(uint16_t address) const {
    if (ioEnabled && address == IO_IN_READY) {
        // Programs can poll this address before reading input. Returning 1
        // means "a byte is ready"; returning 0 means "nothing is available."
        return inputBuffer.empty() ? 0x00 : 0x01;
    }

    if (ioEnabled && address == IO_IN) {
        // Reading the input data register consumes one byte. If nothing is
        // queued, return 0 so small programs have a predictable value.
        if (inputBuffer.empty()) {
            return 0x00;
        }

        uint8_t value = inputBuffer.front();
        inputBuffer.pop_front();
        return value;
    }

    return data[address];
}

void Memory::write(uint16_t address, uint8_t value) {
    if (ioEnabled && address == IO_OUT) {
        // Writing to the output register prints one character. The byte is also
        // stored in data[] so the debugger/tests can inspect the last value.
        if (output) {
            *output << static_cast<char>(value);
            output->flush();
        }
        data[address] = value;
        return;
    }

    data[address] = value;
}
