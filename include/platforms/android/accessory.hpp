#pragma once
#include <cstddef>
#include <cstdint>
#include <se_export.hpp>
#include <string>

/**
 * Android Open Accessory (AOA) link.
 *
 * In AOA the Android device is the USB *device* and the accessory (usually a microcontroller) is the USB host, which
 * switches the device into accessory mode. The Java side (`AccessoryBridge`) takes care of the parts only Java can do
 * (permission, `UsbManager.openAccessory`) and hands the resulting file descriptor to `attach()`. Everything below is
 * plain C++ on top of that descriptor, so native extensions (`.so` files loaded at runtime) can use it too.
 *
 * Reading and writing never block the caller: a thread per direction does the actual I/O.
 */
namespace AndroidAccessory {

struct Info {
    std::string manufacturer;
    std::string model;
    std::string description;
    std::string version;
    std::string uri;
    std::string serial;
};

/// Whether an accessory is currently attached and open.
SE_EXPORT bool isConnected();

/// What the accessory told the device about itself (empty if nothing was ever attached).
SE_EXPORT Info getInfo();

/// Number of received bytes waiting to be read.
SE_EXPORT size_t available();

/// Copies up to `max` received bytes into `data` and removes them from the receive buffer. Returns how many.
SE_EXPORT size_t read(uint8_t *data, size_t max);

/// Whether the receive buffer holds a complete line (terminated by '\n').
SE_EXPORT bool hasLine();

/// Removes the next complete line from the receive buffer (without '\n' or a '\r' before it). False if there is none.
SE_EXPORT bool readLine(std::string &line);

/// Throws away everything received so far.
SE_EXPORT void clearInput();

/// Queues data to be sent to the accessory. False if no accessory is connected.
SE_EXPORT bool write(const uint8_t *data, size_t size);

/// Total number of bytes that didn't fit in the receive buffer and were dropped (since the accessory attached).
SE_EXPORT size_t droppedBytes();

/**
 * Starts using an opened accessory. Takes ownership of `fd`. Called by the Java side; replaces any previous accessory.
 */
SE_EXPORT void attach(int fd, const Info &info);

/// Stops using the current accessory (it was unplugged or the app is done with it).
SE_EXPORT void detach();

} // namespace AndroidAccessory
