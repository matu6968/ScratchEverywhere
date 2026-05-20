#pragma once
#include <string>

namespace Log {
void log(std::string message, bool printToScreen = true);
void logWarning(std::string message, bool printToScreen = true);
void logError(std::string message, bool printToScreen = true);
void writeToFile(std::string message);
void disableFileLogging();
void deleteLogFile();
} // namespace Log