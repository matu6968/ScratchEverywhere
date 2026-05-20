#include "settings.hpp"
#include <filesystem.hpp>
#include <fstream>
#include <log.hpp>
#include <os.hpp>

void SettingsManager::migrate() {
    auto potentialError = FileSystem::createDirectory(OS::getConfigFolderLocation());
    if (!potentialError.has_value()) {
        Log::logError("Could not make config directory: " + potentialError.error());
        return;
    }

    if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation() && FileSystem::fileExists(OS::getScratchFolderLocation() + "Settings.json")) {
        FileSystem::renameFile(OS::getScratchFolderLocation() + "Settings.json", OS::getConfigFolderLocation() + "Settings.json");
    }
}

JsonDocument SettingsManager::getConfigSettings() {
    migrate();

    bool ok = false;
    JsonDocument json = JsonDocument::parseFile(OS::getConfigFolderLocation() + "Settings.json", ok);
    if (!ok) return JsonDocument::object();
    if (!json.root.is_object()) return JsonDocument::object();
    return json;
}

void SettingsManager::saveConfigSettings(const JsonDocument &json) {
    std::ofstream outFile(OS::getConfigFolderLocation() + "Settings.json");
    outFile << json.dump(4);
    outFile.close();
}

JsonDocument SettingsManager::getProjectSettings(const std::string &projectName) {
    bool ok = false;
    JsonDocument json = JsonDocument::parseFile(OS::getScratchFolderLocation() + projectName + ".sb3.json", ok, true);
    if (!ok || !json.root.is_object()) json = JsonDocument::object();
    if (!json.contains("settings") || !json["settings"].is_object()) json["settings"] = JsonValue::makeObject();
    return json;
}

void SettingsManager::saveProjectSettings(const JsonDocument &json, const std::string &projectName) {
    std::ofstream outFile(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    outFile << json.dump(4);
    outFile.close();
}
