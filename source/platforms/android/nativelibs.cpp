#include <SDL.h>
#include <fstream>
#include <log.hpp>
#include <nativelibs.hpp>
#include <sys/stat.h>
#include <unistd.h>

std::string AndroidNativeLibs::stage(const std::string &path) {
    const char *internalStorage = SDL_AndroidGetInternalStoragePath();
    if (!internalStorage) {
        Log::logError("Android native libraries: no private storage available");
        return "";
    }

    const std::string directory = std::string(internalStorage) + "/extensions/";
    mkdir(directory.c_str(), 0700);

    const size_t slash = path.find_last_of('/');
    const std::string destination = directory + (slash == std::string::npos ? path : path.substr(slash + 1));

    struct stat source;
    if (stat(path.c_str(), &source) != 0) {
        Log::logError("Android native libraries: can't read " + path);
        return "";
    }

    struct stat existing;
    if (stat(destination.c_str(), &existing) == 0 && existing.st_size == source.st_size && existing.st_mtime >= source.st_mtime) {
        return destination; // Already up to date
    }

    // Libraries are kept read-only (a requirement for loading dynamic code on newer Android versions), so replace
    // the old copy instead of writing over it.
    unlink(destination.c_str());

    {
        std::ifstream in(path, std::ios::binary);
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (!in || !out) {
            Log::logError("Android native libraries: can't copy " + path + " to " + destination);
            return "";
        }
        out << in.rdbuf();
        if (!out) {
            Log::logError("Android native libraries: writing " + destination + " failed");
            unlink(destination.c_str());
            return "";
        }
    }

    chmod(destination.c_str(), 0400);
    Log::log("Android native libraries: copied " + path + " to " + destination);
    return destination;
}
