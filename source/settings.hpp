#pragma once
#include "json_document.hpp"

namespace SettingsManager {
void migrate();

JsonDocument getConfigSettings();
void saveConfigSettings(const JsonDocument &json);

JsonDocument getProjectSettings(const std::string &projectName);
void saveProjectSettings(const JsonDocument &json, const std::string &projectName);

}; // namespace SettingsManager
