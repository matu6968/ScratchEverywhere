#include "unpackMenu.hpp"
#include "json_document.hpp"
#include "translation.hpp"
#include <filesystem.hpp>
#include <log.hpp>

UnpackMenu::UnpackMenu() {
    init();
}

UnpackMenu::~UnpackMenu() {
    cleanup();
}

void UnpackMenu::init() {
    Render::renderMode = Render::BOTH_SCREENS;

    infoText = createTextObject(TranslationManager::getTranslation("ui.unpack.wait"), 200.0, 100.0);
    infoText->setScale(1.5f);
    infoText->setCenterAligned(true);
    descText = createTextObject(TranslationManager::getTranslation("ui.unpack.warning"), 200.0, 150.0);
    descText->setScale(0.8f);
    descText->setCenterAligned(true);
}

void UnpackMenu::render() {
    Render::beginFrame(0, 181, 165, 111);
    infoText->render(200, 110);
    descText->render(200, 150);

    Render::beginFrame(1, 181, 165, 111);

    Render::endFrame();
}

void UnpackMenu::cleanup() {
    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);
    Render::endFrame();
    Render::renderMode = Render::BOTH_SCREENS;
}

void UnpackMenu::addToJsonArray(const std::string &filePath, const std::string &value) {
    bool ok = false;
    JsonDocument document = JsonDocument::parseFile(filePath, ok, false);
    if (!ok) document = JsonDocument::object();

    if (!document.contains("items") || !document["items"].is_array()) {
        document["items"] = JsonValue::makeArray();
    }

    document["items"].push_back(JsonValue::makeString(value));

    FileSystem::createDirectory(FileSystem::parentPath(filePath));

    std::ofstream outFile(filePath);
    if (!outFile) {
        Log::logError("Failed to write JSON file: " + filePath);
        return;
    }
    outFile << document.dump(2);
    outFile.close();
}

std::vector<std::string> UnpackMenu::getJsonArray(const std::string &filePath) {
    std::vector<std::string> result;
    bool ok = false;
    JsonDocument document = JsonDocument::parseFile(filePath, ok, false);
    if (!ok) return result;

    if (document.contains("items") && document["items"].is_array()) {
        for (const auto &element : document["items"].arrayValue) {
            if (element.is_string()) result.push_back(element.get_string());
        }
    }
    return result;
}

void UnpackMenu::removeFromJsonArray(const std::string &filePath, const std::string &value) {
    bool ok = false;
    JsonDocument document = JsonDocument::parseFile(filePath, ok, false);
    if (!ok || !document.contains("items") || !document["items"].is_array()) return;

    JsonValue filtered = JsonValue::makeArray();
    for (const auto &element : document["items"].arrayValue) {
        if (element.is_string() && element.get_string() == value) continue;
        filtered.push_back(element);
    }
    document["items"] = filtered;

    std::ofstream outFile(filePath);
    if (!outFile) return;
    outFile << document.dump(2);
    outFile.close();
}
