#pragma once

#include <iosfwd>
#include <miniz.h>
#include <os.hpp>
#include <parser.hpp>
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

    static void openScratchProject(void *arg);
    static std::vector<std::string> getProjectFiles(const std::string &directory);

    /**
     * Checks if the path ends with a known Scratch project archive extension (.sb3, or a mod equivalent like .pmp).
     */
    static bool hasProjectExtension(const std::string &path);

    /**
     * Resolves a project path without extension to an existing project file by trying every known archive extension.
     * Falls back to `basePath + ".sb3"` if no file is found.
     */
    static std::string resolveProjectFile(const std::string &basePath);
    static void *getFileInSB3(const std::string &fileName, size_t *outSize = nullptr);
    static nlohmann::json unzipProject(std::istream *file);
    static int openFile(std::istream *&file);
    static bool load();
    static bool extractProject(const std::string &zipPath, const std::string &destFolder);
    static bool deleteProjectFolder(const std::string &directory);
    static nlohmann::json getSetting(const std::string &settingName);
};
