#include <accessory.hpp>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <log.hpp>
#include <memory>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

// The accessory driver moves data in chunks of this size, reads and writes should use it too.
constexpr size_t chunkSize = 16384;
// Limits so a project that never reads (or an accessory that never stops talking) can't eat all the memory.
constexpr size_t maxReceiveBytes = 1024 * 1024;
constexpr size_t maxSendBytes = 1024 * 1024;

/**
 * One accessory connection. The descriptor is closed as soon as both I/O threads are done with it, and not later: the
 * accessory driver can only be opened once, so holding on to a dead connection's descriptor keeps the next accessory
 * from being attached. (A thread can sit in read() for as long as the accessory is plugged in, the driver has no
 * poll(), so a descriptor is never closed underneath one.)
 */
struct Link {
    std::atomic<int> fd{-1};
    std::atomic<int> runningThreads{2};
    AndroidAccessory::Info info;
    std::atomic<bool> alive{true};

    std::mutex mutex; // guards everything below
    std::condition_variable sendReady;
    std::string received;
    std::deque<std::vector<uint8_t>> toSend;
    size_t toSendBytes = 0;
    size_t dropped = 0;

    ~Link() { closeDescriptor(); }

    void closeDescriptor() {
        const int descriptor = fd.exchange(-1);
        if (descriptor >= 0) ::close(descriptor);
    }

    // Called by the reader and the writer when they stop; the last one out closes the descriptor
    void threadFinished() {
        if (--runningThreads == 0) closeDescriptor();
    }

    void kill() {
        alive = false;
        std::lock_guard<std::mutex> lock(mutex);
        sendReady.notify_all();
    }
};

std::mutex currentMutex;
std::shared_ptr<Link> current;

std::shared_ptr<Link> getCurrent() {
    std::lock_guard<std::mutex> lock(currentMutex);
    return current;
}

void readLoop(std::shared_ptr<Link> link) {
    std::vector<uint8_t> buffer(chunkSize);

    while (link->alive) {
        const ssize_t count = ::read(link->fd.load(), buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            break; // ENODEV and friends: the accessory is gone
        }
        if (count == 0) {
            // A zero length packet; don't spin if the descriptor turns out to be at its end for good
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::lock_guard<std::mutex> lock(link->mutex);
        const size_t room = maxReceiveBytes - std::min(maxReceiveBytes, link->received.size());
        const size_t keep = std::min(static_cast<size_t>(count), room);
        link->received.append(reinterpret_cast<const char *>(buffer.data()), keep);
        link->dropped += static_cast<size_t>(count) - keep;
    }

    if (link->alive.exchange(false)) Log::log("Android accessory: connection closed");
    link->kill();
    link->threadFinished();
}

bool writeAll(int fd, const uint8_t *data, size_t size) {
    while (size > 0) {
        const ssize_t count = ::write(fd, data, std::min(size, chunkSize));
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data += count;
        size -= static_cast<size_t>(count);
    }
    return true;
}

void writeLoopBody(const std::shared_ptr<Link> &link);

void writeLoop(std::shared_ptr<Link> link) {
    writeLoopBody(link);
    link->threadFinished();
}

void writeLoopBody(const std::shared_ptr<Link> &link) {
    std::unique_lock<std::mutex> lock(link->mutex);
    while (true) {
        link->sendReady.wait(lock, [&] { return !link->alive || !link->toSend.empty(); });
        if (!link->alive) break;

        std::vector<uint8_t> data = std::move(link->toSend.front());
        link->toSend.pop_front();
        link->toSendBytes -= data.size();

        lock.unlock();
        const int descriptor = link->fd.load();
        const bool ok = descriptor >= 0 && writeAll(descriptor, data.data(), data.size());
        lock.lock();

        if (!ok) {
            if (link->alive.exchange(false)) Log::logWarning("Android accessory: writing failed, closing the connection");
            break;
        }
    }
}

} // namespace

namespace AndroidAccessory {

bool isConnected() {
    const auto link = getCurrent();
    return link && link->alive;
}

Info getInfo() {
    const auto link = getCurrent();
    return link ? link->info : Info{};
}

size_t available() {
    const auto link = getCurrent();
    if (!link) return 0;
    std::lock_guard<std::mutex> lock(link->mutex);
    return link->received.size();
}

size_t read(uint8_t *data, size_t max) {
    const auto link = getCurrent();
    if (!link) return 0;
    std::lock_guard<std::mutex> lock(link->mutex);
    const size_t count = std::min(max, link->received.size());
    std::copy_n(link->received.begin(), count, data);
    link->received.erase(0, count);
    return count;
}

bool hasLine() {
    const auto link = getCurrent();
    if (!link) return false;
    std::lock_guard<std::mutex> lock(link->mutex);
    return link->received.find('\n') != std::string::npos;
}

bool readLine(std::string &line) {
    const auto link = getCurrent();
    if (!link) return false;
    std::lock_guard<std::mutex> lock(link->mutex);

    const size_t end = link->received.find('\n');
    if (end == std::string::npos) return false;

    line = link->received.substr(0, end);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    link->received.erase(0, end + 1);
    return true;
}

void clearInput() {
    const auto link = getCurrent();
    if (!link) return;
    std::lock_guard<std::mutex> lock(link->mutex);
    link->received.clear();
}

bool write(const uint8_t *data, size_t size) {
    const auto link = getCurrent();
    if (!link || !link->alive) return false;
    if (size == 0) return true;

    std::lock_guard<std::mutex> lock(link->mutex);
    if (link->toSendBytes + size > maxSendBytes) return false;
    link->toSend.emplace_back(data, data + size);
    link->toSendBytes += size;
    link->sendReady.notify_one();
    return true;
}

size_t droppedBytes() {
    const auto link = getCurrent();
    if (!link) return 0;
    std::lock_guard<std::mutex> lock(link->mutex);
    return link->dropped;
}

void attach(int fd, const Info &info) {
    auto link = std::make_shared<Link>();
    link->fd = fd;
    link->info = info;

    std::shared_ptr<Link> previous;
    {
        std::lock_guard<std::mutex> lock(currentMutex);
        previous = current;
        current = link;
    }
    if (previous) previous->kill();

    Log::log("Android accessory: attached " + info.manufacturer + " " + info.model + " " + info.version);
    std::thread(readLoop, link).detach();
    std::thread(writeLoop, link).detach();
}

void detach() {
    // The link stays around so what was already received can still be read
    const auto link = getCurrent();
    if (!link) return;
    if (link->alive.exchange(false)) Log::log("Android accessory: detached");
    link->kill();
}

} // namespace AndroidAccessory
