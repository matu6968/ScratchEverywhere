#pragma once
#include <json_dom.hpp>
#include <sprite.hpp>

struct Parser {

    static bool logParsing;

    static void loadUsernameFromSettings();

    static void loadSprites(simdjson::dom::element json);
    static bool loadExtensions(simdjson::dom::element json);

#ifdef ENABLE_CLOUDVARS
    static void initMist();
    static void shutdownCloud();
#endif
  private:
    static void log(const std::string &message);

    static Block *loadBlock(Sprite *newSprite, const std::string id, simdjson::dom::element blockDatas, Block *parentBlock, int indent);
    static void loadFields(Block &block, const std::string &blockKey, simdjson::dom::element blockDatas, int indent);
    static void loadInputs(Block &block, Sprite *newSprite, std::string blockKey, simdjson::dom::element blockDatas, int indent);
    static void loadAdvancedProjectSettings(simdjson::dom::element json);
    static void setSubstack(Block *startBlock, Block *stopBlock = nullptr);

}; // namespace Parser
