#include <log.hpp>
#include <os.hpp>

#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <__getbasepath/internal.h>
#include <SDL2/SDL_system.h>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "Android";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "/storage/emulated/0";
}

std::string OS::getConfigFolderLocation() {
    const char *basepath = SDL_AndroidGetExternalStoragePath();
    std::string cpp_basepath = basepath ? basepath : "";
    return cpp_basepath + "config/";
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;

    const char *basepath = SDL_AndroidGetExternalStoragePath();
    std::string cpp_basepath = basepath ? basepath : "";
    return cpp_basepath + "scratch-everywhere/";
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    return "Player";
}
