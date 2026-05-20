#include "translation.hpp"
#include "json_document.hpp"
#include "os.hpp"
#include "settings.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

static std::unordered_map<std::string, std::string> translationKeys;
static std::vector<std::string> splashTexts;
static TranslationManager::LanguageInfo loadedLanguage;

const TranslationManager::LanguageInfo &TranslationManager::getLoadedLanguage() {
    return loadedLanguage;
}

const std::vector<TranslationManager::LanguageInfo> TranslationManager::getLanguages() {
    std::vector<LanguageInfo> ret;

    const std::string path = OS::getRomFSLocation() + "gfx/translations/languages.json";
    bool ok = false;
    std::string content;
#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(path);
    content.assign(file.begin(), file.end());
#else
    std::ifstream file(path);
    content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
#endif

    JsonDocument json = JsonDocument::parseContent(content, ok);
    if (!ok || !json.root.is_object()) return ret;

    for (const auto &[key, name] : json.root.objectValue) {
        ret.push_back({static_cast<unsigned int>(ret.size()), key, name.is_string() ? name.get_string() : key});
    }

    return ret;
}

void TranslationManager::loadLanguage(std::string language) {
    if (language == "") language = SettingsManager::getConfigSettings().valueString("Language", "en_us");

    const auto &languages = getLanguages();
    loadedLanguage = *std::find_if(languages.begin(), languages.end(), [&language](LanguageInfo info) { return info.key == language; });

    const std::string path = OS::getRomFSLocation() + "gfx/translations/" + language + ".json";
    const std::string splashPath = OS::getRomFSLocation() + "gfx/translations/" + language + ".splashes.txt";

    splashTexts.clear();
    translationKeys.clear();

#ifdef USE_CMAKERC
    const auto &fs = cmrc::romfs::get_filesystem();

    const auto &file = fs.open(path);
    bool ok = false;
    JsonDocument json = JsonDocument::parseContent(std::string(file.begin(), file.end()), ok);
    if (ok && json.root.is_object()) {
        for (const auto &[key, value] : json.root.objectValue) {
            if (value.is_string()) translationKeys[key] = value.get_string();
        }
    }

    const auto &splashFile = fs.open(splashPath);
    std::string_view sv(splashFile.begin(), splashFile.size());
    std::istringstream stream{std::string(sv)};

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            splashTexts.push_back(line);
        }
    }
#else
    bool ok = false;
    JsonDocument json = JsonDocument::parseFile(path, ok, true);
    if (ok && json.root.is_object()) {
        for (const auto &[key, value] : json.root.objectValue) {
            if (value.is_string()) translationKeys[key] = value.get_string();
        }
    }

    std::ifstream splashFile(splashPath);
    std::string line;
    while (std::getline(splashFile, line)) {
        if (!line.empty()) {
            splashTexts.push_back(line);
        }
    }
#endif
}

const std::string TranslationManager::getTranslation(const std::string &translationKey) {
    auto it = translationKeys.find(translationKey);
    if (it == translationKeys.end()) return translationKey;
    return it->second;
}

const std::string TranslationManager::getSplashText() {
    constexpr const char *fallback = "Everywhere!";

    if (splashTexts.empty()) {
        return fallback;
    }

    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
    std::uniform_int_distribution<size_t> dist(0, splashTexts.size() - 1);

    std::string splash = splashTexts[dist(rng)];

    // Replace {PlatformName} and {UserName} placeholders with actual values
    const std::string platformPlaceholder = "{PlatformName}";
    const std::string platform = OS::getPlatform();

    const std::string usernamePlaceholder = "{UserName}";
    std::string username = OS::getUsername();
    JsonDocument json = SettingsManager::getConfigSettings();
    if (json.contains("EnableUsername") && json["EnableUsername"].is_bool() && json["EnableUsername"].get_bool()) {
        if (json.contains("Username") && json["Username"].is_string()) {
            std::string customUsername = json["Username"].get_string();
            if (!customUsername.empty()) {
                username = customUsername;
            }
        }
    }

    size_t pos = 0;

    while ((pos = splash.find(platformPlaceholder, pos)) != std::string::npos) {
        splash.replace(pos, platformPlaceholder.size(), platform);
        pos += platform.size(); // move past replacement
    }

    pos = 0;
    while ((pos = splash.find(usernamePlaceholder, pos)) != std::string::npos) {
        splash.replace(pos, usernamePlaceholder.size(), username);
        pos += username.size(); // move past replacement
    }

    return splash;
}
