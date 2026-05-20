#pragma once

#include <iosfwd>
#include <json_document.hpp>
#include <miniz.h>
#include <os.hpp>
#include <simdjson.h>
#include <string>
#include <vector>

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

class Unzip {
  public:
    static volatile int projectOpened;
    static std::string loadingState;
    static volatile bool threadFinished;
    static std::string filePath;
    static bool UnpackedInSD;
    static mz_zip_archive zipArchive;
    static std::vector<char> zipBuffer;
    static simdjson::dom::parser projectParser;
    static simdjson::dom::parser nestedParser;
    static bool projectJsonValid;

    static void openScratchProject(void *arg);
    static std::vector<std::string> getProjectFiles(const std::string &directory);
    static void *getFileInSB3(const std::string &fileName, size_t *outSize = nullptr);
    static simdjson::dom::element unzipProject(std::istream *file);
    static int openFile(std::istream *&file);
    static bool load();
    static bool extractProject(const std::string &zipPath, const std::string &destFolder);
    static bool deleteProjectFolder(const std::string &directory);
    static JsonValue getSetting(const std::string &settingName);
};
