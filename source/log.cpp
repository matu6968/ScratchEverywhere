#include <iostream>
#include <log.hpp>
#include <render.hpp>

#if defined(__PS4__)
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#endif

#if defined(__ANDROID__)
#include <SDL2/SDL_system.h>
#include <android/log.h>
#define LOGCAT_TAG "ScratchEverywhere"
#endif

// PS4 implementation of logging
#ifdef __PS4__
char logBuffer[1024];

void Log::log(std::string message) {

    snprintf(logBuffer, 1023, "<SE!> %s\n", message.c_str());
    sceKernelDebugOutText(0, logBuffer);
}
void Log::logWarning(std::string message) {
    snprintf(logBuffer, 1023, "<SE!> Warning: %s\n", message.c_str());
    sceKernelDebugOutText(0, logBuffer);
}
void Log::logError(std::string message) {
    snprintf(logBuffer, 1023, "<SE!> Error: %s\n", message.c_str());
    sceKernelDebugOutText(0, logBuffer);
}
void Log::writeToFile(std::string message) {
}

void Log::deleteLogFile() {
}
// Android implementation of logging
#elif defined(__ANDROID__)
void Log::log(std::string message) {
    __android_log_print(ANDROID_LOG_INFO, LOGCAT_TAG, "%s", message.c_str());
    writeToFile(message);
}

void Log::logWarning(std::string message) {
    __android_log_print(ANDROID_LOG_WARN, LOGCAT_TAG, "%s", message.c_str());
    writeToFile("Warning: " + message);
}

void Log::logError(std::string message) {
    __android_log_print(ANDROID_LOG_ERROR, LOGCAT_TAG, "%s", message.c_str());
    writeToFile("Error: " + message);
}

void Log::writeToFile(std::string message) {
    if (Render::debugMode) {
        std::string filePath = OS::getScratchFolderLocation() + "log.txt";
        std::ofstream logFile;
        logFile.open(filePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        } else {
            std::cerr << "Could not open log file: " << filePath << std::endl;
        }
    }
}

void Log::deleteLogFile() {
    std::string filePath = OS::getScratchFolderLocation() + "/log.txt";
    if (std::remove(filePath.c_str()) != 0) {
        Log::logError("Failed to delete log file: " + std::string(std::strerror(errno)));
    }
}
#else
static std::string lastLog;
void Log::log(std::string message) {
    if (lastLog == message) return;
    lastLog = message;
    std::cout << message << std::endl;
    writeToFile(message);
}

void Log::logWarning(std::string message) {
    if (lastLog == message) return;
    lastLog = message;
    std::cout << "\x1b[1;33m" << "Warning: " << message << "\x1b[0m" << std::endl;
    writeToFile("<Warning> " + message);
}

void Log::logError(std::string message) {
    if (lastLog == message) return;
    lastLog = message;
    std::cerr << "\x1b[1;31m" << "Error: " << message << "\x1b[0m" << std::endl;

    writeToFile("<Error> " + message);
}

void Log::writeToFile(std::string message) {
    if (Render::debugMode) {
        std::string filePath = OS::getScratchFolderLocation() + "log.txt";
        std::ofstream logFile;
        logFile.open(filePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        } else {
            std::cerr << "Could not open log file: " << filePath << std::endl;
        }
    }
}

void Log::deleteLogFile() {
    std::string filePath = OS::getScratchFolderLocation() + "/log.txt";
    if (std::remove(filePath.c_str()) != 0) {
        Log::logError("Failed to delete log file: " + std::string(std::strerror(errno)));
    }
}

#endif