// Android only extension.
#ifdef __ANDROID__
#include <log.hpp>
#include "blockUtils.hpp"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <accessory.hpp>

namespace {
bool isConnected() { return AndroidAccessory::isConnected(); }

bool send(const std::string &data) {
    return AndroidAccessory::write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

// "72 101,108 0x6c" -> {72, 101, 108, 108}. Anything that isn't a number between 0 and 255 is skipped.
std::vector<uint8_t> parseBytes(const std::string &text) {
    std::vector<uint8_t> bytes;
    const char *position = text.c_str();
    while (*position) {
        if (std::isspace(static_cast<unsigned char>(*position)) || *position == ',') {
            position++;
            continue;
        }
        char *end = nullptr;
        const long value = std::strtol(position, &end, 0);
        if (end == position) {
            position++; // not a number, skip the character
            continue;
        }
        if (value >= 0 && value <= 255) bytes.push_back(static_cast<uint8_t>(value));
        position = end;
    }
    return bytes;
}

} // namespace

SCRATCH_BLOCK(aoa, connected) {
    *outValue = Value(isConnected());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, info) {
    Value what;
    if (!Scratch::getInputValue(block, "WHAT", thread, sprite, what))
        return BlockResult::REPEAT;

    std::string result;
    const AndroidAccessory::Info info = AndroidAccessory::getInfo();
    const std::string key = what.asString();
    if (key == "manufacturer") result = info.manufacturer;
    else if (key == "model") result = info.model;
    else if (key == "description") result = info.description;
    else if (key == "version") result = info.version;
    else if (key == "uri") result = info.uri;
    else if (key == "serial") result = info.serial;
    
    *outValue = Value(result);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, send) {
    Value text;
    if (!Scratch::getInputValue(block, "TEXT", thread, sprite, text))
        return BlockResult::REPEAT;

    send(text.asString());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, sendLine) {
    Value text;
    if (!Scratch::getInputValue(block, "TEXT", thread, sprite, text))
        return BlockResult::REPEAT;

    send(text.asString() + "\n");
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, sendBytes) {
    Value text;
    if (!Scratch::getInputValue(block, "BYTES", thread, sprite, text))
        return BlockResult::REPEAT;

    const std::vector<uint8_t> bytes = parseBytes(text.asString());

    if (!bytes.empty()) AndroidAccessory::write(bytes.data(), bytes.size());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, available) {
    *outValue = Value(static_cast<double>(AndroidAccessory::available()));
    
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, lineAvailable) {
    *outValue = Value(AndroidAccessory::hasLine());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, readLine) {
    std::string line;

    AndroidAccessory::readLine(line);

    *outValue = Value(line);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, readByte) {
    int result = -1;

    uint8_t byte;
    if (AndroidAccessory::read(&byte, 1) == 1) result = byte;
    *outValue = Value(result);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, readAll) {
    std::string text;

    std::vector<uint8_t> buffer(AndroidAccessory::available());
    buffer.resize(AndroidAccessory::read(buffer.data(), buffer.size()));
    text.assign(buffer.begin(), buffer.end());
    *outValue = Value(text);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, clear) {

    AndroidAccessory::clearInput();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(aoa, dropped) {

    *outValue = Value(static_cast<double>(AndroidAccessory::droppedBytes()));

    return BlockResult::CONTINUE;
}
#else
SCRATCH_SHADOW_BLOCK(aoa, connected)
SCRATCH_SHADOW_BLOCK(aoa, info)
SCRATCH_SHADOW_BLOCK(aoa, send)
SCRATCH_SHADOW_BLOCK(aoa, sendLine)
SCRATCH_SHADOW_BLOCK(aoa, sendBytes)
SCRATCH_SHADOW_BLOCK(aoa, available)
SCRATCH_SHADOW_BLOCK(aoa, lineAvailable)
SCRATCH_SHADOW_BLOCK(aoa, readLine)
SCRATCH_SHADOW_BLOCK(aoa, readByte)
SCRATCH_SHADOW_BLOCK(aoa, readAll)
SCRATCH_SHADOW_BLOCK(aoa, clear)
SCRATCH_SHADOW_BLOCK(aoa, dropped)
#endif
