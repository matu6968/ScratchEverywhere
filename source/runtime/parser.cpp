#include "parser.hpp"
#include "sprite.hpp"
#include <algorithm>
#include <filesystem.hpp>
#include <input.hpp>
#include <limits>
#include <log.hpp>
#include <math.hpp>
#include <memory>
#include <os.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <settings.hpp>
#include <unordered_map>
#include <unzip.hpp>
#if defined(__WIIU__) && defined(ENABLE_CLOUDVARS)
#include <whb/sdcard.h>
#endif

#ifdef ENABLE_CUSTOM_EXTENSIONS
#include <extensions/interface.hpp>
#include <extensions/meta.hpp>
#endif
#ifdef ENABLE_NATIVE_EXTENSIONS
#include <dlfcn.h>
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

#ifdef ENABLE_CLOUDVARS
#include <fstream>
#include <mist/mist.hpp>
#include <random>
#include <sstream>

const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;

std::string Scratch::cloudUsername;
bool Scratch::cloudProject = false;

std::unique_ptr<MistConnection> cloudConnection = nullptr;
#endif

#ifdef ENABLE_CLOUDVARS
void Parser::initMist() {
    OS::initWifi();

    const std::string usernameFilename = OS::getScratchFolderLocation() + "cloud-username.txt";

    std::ifstream fileStream(usernameFilename.c_str());
    if (!fileStream.good()) {
        std::random_device rd;
        std::ostringstream usernameStream;
        usernameStream << "player" << std::setw(7) << std::setfill('0') << rd() % 10000000;
        Scratch::cloudUsername = usernameStream.str();
        std::ofstream usernameFile;
        usernameFile.open(usernameFilename);
        usernameFile << Scratch::cloudUsername;
        usernameFile.close();
    } else {
        fileStream >> Scratch::cloudUsername;
    }
    fileStream.close();

    std::vector<std::string> assetIds;
    for (const auto &sprite : Scratch::sprites) {
        for (const auto &costume : sprite->costumes) {
            assetIds.push_back(costume.id);
        }
        for (const auto &sound : sprite->sounds) {
            assetIds.push_back(sound.id);
        }
    }

    uint64_t assetHash = 0;
    for (const auto &assetId : assetIds) {
        uint64_t hash = FNV_OFFSET_BASIS_64;
        for (char c : assetId) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME_64;
        }

        assetHash += hash;
    }

    std::ostringstream projectID;
    projectID << "ScratchEverywhere/hash-" << std::hex << std::setw(16) << std::setfill('0') << assetHash;
    cloudConnection = std::make_unique<MistConnection>(projectID.str(), Scratch::cloudUsername, "contact@grady.link");

    cloudConnection->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            Log::log("Mist++ Connected: " + message);
            return;
        }
        Log::log("Mist++ Disconnected: " + message);
    });

    cloudConnection->onVariableUpdate(BlockExecutor::handleCloudVariableChange);

    Log::log("Connecting to cloud variables with id: " + projectID.str());
#if defined(__PC__) && !(defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__))
    cloudConnection->connect();
#else
    cloudConnection->connect(false);
#endif
}

void Parser::shutdownCloud() {
    cloudConnection.reset();
    Scratch::cloudProject = false;
}
#endif

std::unordered_map<std::string, std::string> &Parser::getShadowBlocks() {
    static std::unordered_map<std::string, std::string> shadowBlocks;
    return shadowBlocks;
}

void Parser::loadUsernameFromSettings() {
    Scratch::customUsername = "Player";
    Scratch::useCustomUsername = false;

    JsonDocument j = SettingsManager::getConfigSettings();

    if (j.contains("EnableUsername") && j["EnableUsername"].is_bool()) {
        Scratch::useCustomUsername = j["EnableUsername"].get_bool();
    }

    if (j.contains("Username") && j["Username"].is_string()) {
        bool hasNonSpace = false;
        for (char c : j["Username"].get_string()) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                hasNonSpace = true;
            } else if (!std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
        }
        if (hasNonSpace) Scratch::customUsername = j["Username"].get_string();
        else Scratch::customUsername = "Player";
    }
}

bool Parser::logParsing = false;

void Parser::log(const std::string &message) {
    if (Parser::logParsing) {
        Log::log(message);
    }
}

void Parser::loadSprites(simdjson::dom::element json) {
    Parser::logParsing = false; // ToDo: Activate it via Settings (Only if Logs in generall are enabled)
    simdjson::dom::element spritesData;
    if (json["targets"].get(spritesData)) return;

    int spriteAmount = static_cast<int>(JsonDom::arraySize(spritesData));
    Scratch::sprites.reserve(spriteAmount);

    simdjson::dom::array targetsArray;
    if (spritesData.get_array().get(targetsArray)) return;

    for (simdjson::dom::element target : targetsArray) {
        Sprite *newSprite = new Sprite();

        // Basic properties
        if (auto name = JsonDom::getString(target, "name")) {
            newSprite->name = *name;
        }
        if (JsonDom::hasKey(target, "isStage")) {
            newSprite->isStage = JsonDom::getBool(target, "isStage").value_or(false);
            if (newSprite->isStage) loadAdvancedProjectSettings(target);
        }

        Parser::log(newSprite->name + " (" + std::string(newSprite->isStage ? "Stage" : "Sprite") + ")");

        if (JsonDom::hasKey(target, "draggable")) {
            newSprite->draggable = JsonDom::getBool(target, "draggable").value_or(false);
        }
        if (JsonDom::hasKey(target, "visible")) {
            newSprite->visible = JsonDom::getBool(target, "visible").value_or(true);
        } else {
            newSprite->visible = true;
        }
        if (JsonDom::hasKey(target, "currentCostume")) {
            newSprite->currentCostume = JsonDom::getIntOr(target, "currentCostume", 0);
        }
        if (JsonDom::hasKey(target, "volume")) {
            newSprite->volume = JsonDom::getIntOr(target, "volume", 100);
        }
        if (JsonDom::hasKey(target, "x")) {
            newSprite->xPosition = static_cast<float>(JsonDom::getDoubleOr(target, "x", 0));
        }
        if (JsonDom::hasKey(target, "y")) {
            newSprite->yPosition = static_cast<float>(JsonDom::getDoubleOr(target, "y", 0));
        }
        if (JsonDom::hasKey(target, "size")) {
            newSprite->size = static_cast<float>(JsonDom::getDoubleOr(target, "size", 100));
        } else {
            newSprite->size = 100;
        }
        if (JsonDom::hasKey(target, "direction")) {
            newSprite->rotation = static_cast<float>(JsonDom::getDoubleOr(target, "direction", 90));
        } else {
            newSprite->rotation = 90;
        }
        if (JsonDom::hasKey(target, "layerOrder")) {
            newSprite->layer = JsonDom::getIntOr(target, "layerOrder", 0);
        } else {
            newSprite->layer = 0;
        }
        if (JsonDom::hasKey(target, "rotationStyle")) {
            std::string style = JsonDom::getString(target, "rotationStyle").value_or("");
            if (style == "all around")
                newSprite->rotationStyle = newSprite->ALL_AROUND;
            else if (style == "left-right")
                newSprite->rotationStyle = newSprite->LEFT_RIGHT;
            else
                newSprite->rotationStyle = newSprite->NONE;
        }
        newSprite->isClone = false;

        // Variables
        if (JsonDom::hasKey(target, "variables") && !JsonDom::isEmptyObject(target["variables"])) {
            Parser::log("\tVariables:");
            simdjson::dom::object variablesObj;
            if (!target["variables"].get_object().get(variablesObj)) {
                for (auto [id, data] : variablesObj) {
                    Variable newVariable;
                    newVariable.id = std::string(id);
                    newVariable.name = JsonDom::getStringValue(JsonDom::arrayAt(data, 0)).value_or("");
                    newVariable.value = Value::fromJson(JsonDom::arrayAt(data, 1));
#ifdef ENABLE_CLOUDVARS
                    newVariable.cloud = JsonDom::arraySize(data) == 3;
                    Scratch::cloudProject = Scratch::cloudProject || newVariable.cloud;
#endif
                    newSprite->variables[newVariable.id] = newVariable;
                    Parser::log("\t\t" + newVariable.name + " = " + newVariable.value.asString());
                }
            }
        }

        // Lists
        if (JsonDom::hasKey(target, "lists") && !JsonDom::isEmptyObject(target["lists"])) {
            Parser::log("\tLists:");
            simdjson::dom::object listsObj;
            if (!target["lists"].get_object().get(listsObj)) {
                for (auto [id, data] : listsObj) {
                    auto result = newSprite->lists.try_emplace(std::string(id)).first;
                    List &newList = result->second;
                    newList.id = std::string(id);
                    newList.name = JsonDom::getStringValue(JsonDom::arrayAt(data, 0)).value_or("");
                    simdjson::dom::element listItems = JsonDom::arrayAt(data, 1);
                    newList.items.reserve(JsonDom::arraySize(listItems));
                    Parser::log("\t\t" + newList.name + " [" + std::to_string(JsonDom::arraySize(listItems)) + " items]");
                    simdjson::dom::array itemsArray;
                    if (!listItems.get_array().get(itemsArray)) {
                        for (simdjson::dom::element listItem : itemsArray) {
                            newList.items.push_back(Value::fromJson(listItem));
                        }
                    }
                }
            }
        }

        // Sounds
        if (JsonDom::hasKey(target, "sounds") && !JsonDom::isEmptyArray(target["sounds"])) {
            Parser::log("\tSounds:");
            simdjson::dom::array soundsArray;
            if (!target["sounds"].get_array().get(soundsArray)) {
                for (simdjson::dom::element data : soundsArray) {
                    Sound newSound;
                    newSound.id = JsonDom::getString(data, "assetId").value_or("");
                    newSound.name = JsonDom::getString(data, "name").value_or("");
                    newSound.fullName = JsonDom::getString(data, "md5ext").value_or("");
                    newSound.dataFormat = JsonDom::getString(data, "dataFormat").value_or("");
                    newSound.sampleRate = JsonDom::getIntOr(data, "rate", -1);
                    newSound.sampleCount = JsonDom::getIntOr(data, "sampleCount", -1);
                    newSprite->sounds.push_back(newSound);
                    Parser::log("\t\t" + newSound.name);
                }
            }
        }

        // Costumes
        if (JsonDom::hasKey(target, "costumes") && !JsonDom::isEmptyArray(target["costumes"])) {
            Parser::log("\tCostumes:");
            simdjson::dom::array costumesArray;
            if (!target["costumes"].get_array().get(costumesArray)) {
                for (simdjson::dom::element data : costumesArray) {
                    Costume newCostume;
                    newCostume.id = JsonDom::getString(data, "assetId").value_or("");
                    if (JsonDom::hasKey(data, "name")) {
                        newCostume.name = JsonDom::getString(data, "name").value_or("");
                    }
                    if (JsonDom::hasKey(data, "bitmapResolution")) {
                        newCostume.bitmapResolution = JsonDom::getIntOr(data, "bitmapResolution", 1);
                    }
                    if (JsonDom::hasKey(data, "dataFormat")) {
                        newCostume.dataFormat = JsonDom::getString(data, "dataFormat").value_or("");
                        newCostume.isSVG = (newCostume.dataFormat == "svg" || newCostume.dataFormat == "SVG");
                    }
                    if (JsonDom::hasKey(data, "md5ext")) {
                        newCostume.fullName = JsonDom::getString(data, "md5ext").value_or("");
                    }
                    if (JsonDom::hasKey(data, "rotationCenterX")) {
                        newCostume.rotationCenterX = JsonDom::getIntOr(data, "rotationCenterX", 0);
                        if (Scratch::bitmapHalfQuality && !newCostume.isSVG && newCostume.bitmapResolution == 2) newCostume.rotationCenterX /= 2;
                    }
                    if (JsonDom::hasKey(data, "rotationCenterY")) {
                        newCostume.rotationCenterY = JsonDom::getIntOr(data, "rotationCenterY", 0);
                        if (Scratch::bitmapHalfQuality && !newCostume.isSVG && newCostume.bitmapResolution == 2) newCostume.rotationCenterY /= 2;
                    }
                    if (Scratch::bitmapHalfQuality) newCostume.bitmapResolution = 1;
                    newSprite->costumes.push_back(newCostume);
                    Parser::log("\t\t" + newCostume.name);
                }
            }
        }

        // Broadcasts
        if (JsonDom::hasKey(target, "broadcasts") && !JsonDom::isEmptyObject(target["broadcasts"])) {
            simdjson::dom::object broadcastsObj;
            if (!target["broadcasts"].get_object().get(broadcastsObj)) {
                for (auto [id, data] : broadcastsObj) {
                    Broadcast newBroadcast;
                    newBroadcast.id = std::string(id);
                    newBroadcast.name = JsonDom::getStringValue(data).value_or("");
                    newSprite->broadcasts[newBroadcast.id] = newBroadcast;
                }
            }
        }

        std::vector<std::string> procedureCallBlocks;

        if (JsonDom::hasKey(target, "blocks") && !JsonDom::isEmptyObject(target["blocks"])) {
            Parser::log("\tBlocks:");

            simdjson::dom::element blocksData;
            target["blocks"].get(blocksData);
            simdjson::dom::object blocksObj;
            if (blocksData.get_object().get(blocksObj)) {
                blocksObj = {};
            }

            for (auto [id, data] : blocksObj) {
                std::string blockId(id);
                if (JsonDom::hasKey(data, "opcode") && JsonDom::getString(data, "opcode") == "procedures_definition") {
                    procedureCallBlocks.push_back(blockId);
                    continue;
                }

                if (!JsonDom::hasKey(data, "topLevel") || !JsonDom::getBool(data, "topLevel").value_or(false)) continue;
                if (!JsonDom::hasKey(data, "opcode")) continue;

                std::string opcode = JsonDom::getString(data, "opcode").value_or("");
                Block *newBlock = new Block();

                newBlock->opcode = opcode;
                if (newBlock->opcode == "event_whenthisspriteclicked" || newBlock->opcode == "event_whenstageclicked") {
                    newSprite->shouldDoSpriteClick = true;
                }

                if (BlockExecutor::getHandlers().count(opcode) > 0) {
                    newBlock->blockFunction = BlockExecutor::getHandlers()[opcode];
                } else {
                    Parser::log("\t\t! Unknown opcode: " + opcode);
                    newBlock->blockFunction = BlockExecutor::getHandlers()["coreExample_exampleOpcode"];
                }

                Parser::log("\t\t" + opcode);
                loadInputs(*newBlock, newSprite, blockId, blocksData, 2);
                loadFields(*newBlock, blockId, blocksData, 2);

                Scratch::blocks.push_back(newBlock);
                newSprite->hats[opcode].insert(newBlock);

                simdjson::dom::element nextElem;
                if (!JsonDom::hasKey(data, "next") || data["next"].get(nextElem) || nextElem.is_null()) {
                    Parser::log("\t\t\t! No next block");
                } else {
                    std::string nextBlockKey = JsonDom::getStringValue(nextElem).value_or("");
                    newBlock->nextBlock = loadBlock(newSprite, nextBlockKey, blocksData, nullptr, 2);
                }
                setSubstack(newBlock);
            }

            for (const std::string &id : procedureCallBlocks) {
                simdjson::dom::element data;
                if (blocksData[id].get(data)) continue;

                simdjson::dom::element customBlock;
                if (!JsonDom::hasKey(data, "inputs") || data["inputs"]["custom_block"].get(customBlock) ||
                    !customBlock.is_array() || JsonDom::arraySize(customBlock) < 2) {
                    Parser::log("\t\t! procedures_call without custom_block input");
                    continue;
                }

                std::string prototypeId;
                simdjson::dom::element prototypeIdElem = JsonDom::arrayAt(customBlock, 1);
                if (prototypeIdElem.is_string()) {
                    prototypeId = JsonDom::getStringValue(prototypeIdElem).value_or("");
                } else {
                    Parser::log("\t\t! procedures_call prototype block ID is not a string");
                    continue;
                }

                simdjson::dom::element prototype;
                if (blocksData[prototypeId].get(prototype)) {
                    Parser::log("\t\t! procedures_call prototype block not found");
                    continue;
                }

                if (!JsonDom::hasKey(prototype, "mutation")) {
                    Parser::log("\t\t! procedures_call without mutation");
                    continue;
                }
                std::string proccode = JsonDom::getString(prototype["mutation"], "proccode").value_or("");

                if (newSprite->customHatBlock.find(proccode) == newSprite->customHatBlock.end()) {
                    newSprite->customHatBlock[proccode] = new Block();
                    Parser::log("\t\t! Unknown procedure: '" + proccode + "'");
                }
                Parser::log("\t\t! Procedure '" + proccode + "' found");
                Block *definitionBlock = newSprite->customHatBlock[proccode];
                definitionBlock->blockFunction = BlockExecutor::getHandlers()["procedures_prototype"];

                simdjson::dom::element mutation = prototype["mutation"];
                if (JsonDom::hasKey(mutation, "argumentnames") && JsonDom::hasKey(mutation, "argumentids")) {
                    std::string rawArgumentIds = JsonDom::getString(mutation, "argumentids").value_or("");
                    definitionBlock->argumentIDs = JsonDom::parseStringArray(Unzip::nestedParser, rawArgumentIds);

                    std::string rawArgumentNames = JsonDom::getString(mutation, "argumentnames").value_or("");
                    definitionBlock->argumentNames = JsonDom::parseStringArray(Unzip::nestedParser, rawArgumentNames);
                }

                if (JsonDom::hasKey(mutation, "argumentdefaults")) {
                    std::string rawArgumentDefaults = JsonDom::getString(mutation, "argumentdefaults").value_or("");
                    simdjson::dom::element parsedAD;
                    definitionBlock->argumentDefaults.clear();
                    if (JsonDom::parseNested(Unzip::nestedParser, rawArgumentDefaults, parsedAD) && parsedAD.is_array()) {
                        simdjson::dom::array defaultsArray;
                        if (!parsedAD.get_array().get(defaultsArray)) {
                            for (simdjson::dom::element item : defaultsArray) {
                                definitionBlock->argumentDefaults.push_back(Value::fromJson(item));
                            }
                        }
                    }
                }
                if (JsonDom::hasKey(mutation, "warp")) {
                    simdjson::dom::element warp;
                    mutation["warp"].get(warp);
                    bool warpValue = false;
                    if (warp.is_string()) {
                        warpValue = JsonDom::getStringValue(warp) == "true";
                    } else if (warp.is_bool()) {
                        warp.get_bool().get(warpValue);
                    }
                    definitionBlock->MyBlockWithoutScreenRefresh = warpValue;
                } else {
                    definitionBlock->MyBlockWithoutScreenRefresh = false;
                }

                simdjson::dom::element nextElem;
                if (JsonDom::hasKey(data, "next") && !data["next"].get(nextElem) && !nextElem.is_null()) {
                    std::string nextKey = JsonDom::getStringValue(nextElem).value_or("");
                    definitionBlock->nextBlock = loadBlock(newSprite, nextKey, blocksData, nullptr, 2);
                    Parser::log("\t\t! Procedure body loaded from: " + nextKey);
                }
                setSubstack(definitionBlock);
            }
            for (Block *block : Scratch::blocks) {
                if (block->opcode == "procedures_call" && block->MyBlockDefinitionID != nullptr) {
                    block->MyBlockWithoutScreenRefresh =
                        block->MyBlockDefinitionID->MyBlockWithoutScreenRefresh;
                }
            }
        }

        Scratch::sprites.push_back(newSprite);
        if (newSprite->isStage) Scratch::stageSprite = newSprite;
    }

    Scratch::sortSprites();

    if (JsonDom::hasKey(json, "monitors") && json["monitors"].is_array()) {
        Parser::log("Loading monitors:");
        simdjson::dom::array monitorsArray;
        if (!json["monitors"].get_array().get(monitorsArray)) {
            for (simdjson::dom::element monitor : monitorsArray) {
                Monitor newMonitor;

                simdjson::dom::element idElem;
                if (JsonDom::hasKey(monitor, "id") && !monitor["id"].get(idElem) && !idElem.is_null())
                    newMonitor.id = JsonDom::getStringValue(idElem).value_or("");

                simdjson::dom::element modeElem;
                if (JsonDom::hasKey(monitor, "mode") && !monitor["mode"].get(modeElem) && !modeElem.is_null())
                    newMonitor.mode = JsonDom::getStringValue(modeElem).value_or("");

                simdjson::dom::element opcodeElem;
                if (JsonDom::hasKey(monitor, "opcode") && !monitor["opcode"].get(opcodeElem) && !opcodeElem.is_null())
                    newMonitor.opcode = JsonDom::getStringValue(opcodeElem).value_or("");

                if (JsonDom::hasKey(monitor, "params") && monitor["params"].is_object()) {
                    simdjson::dom::object paramsObj;
                    if (!monitor["params"].get_object().get(paramsObj)) {
                        for (auto [key, value] : paramsObj) {
                            newMonitor.parameters[std::string(key)] = JsonDom::toJsonString(value);
                        }
                    }
                }

                if (JsonDom::hasKey(monitor, "spriteName") && monitor["spriteName"].is_string())
                    newMonitor.spriteName = JsonDom::getString(monitor, "spriteName").value_or("");
                else
                    newMonitor.spriteName = "";

                simdjson::dom::element valueElem;
                if (JsonDom::hasKey(monitor, "value") && !monitor["value"].get(valueElem) && !valueElem.is_null())
                    newMonitor.value = Value(Math::removeQuotations(JsonDom::toJsonString(valueElem)));

                simdjson::dom::element xElem;
                if (JsonDom::hasKey(monitor, "x") && !monitor["x"].get(xElem) && !xElem.is_null())
                    newMonitor.x = JsonDom::getIntOr(monitor, "x", 0);

                simdjson::dom::element yElem;
                if (JsonDom::hasKey(monitor, "y") && !monitor["y"].get(yElem) && !yElem.is_null())
                    newMonitor.y = JsonDom::getIntOr(monitor, "y", 0);

                simdjson::dom::element widthElem;
                if (JsonDom::hasKey(monitor, "width") && !monitor["width"].get(widthElem) && !widthElem.is_null() && JsonDom::getIntOr(monitor, "width", 0) != 0)
                    newMonitor.width = JsonDom::getIntOr(monitor, "width", 110);
                else
                    newMonitor.width = 110;

                simdjson::dom::element heightElem;
                if (JsonDom::hasKey(monitor, "height") && !monitor["height"].get(heightElem) && !heightElem.is_null() && JsonDom::getIntOr(monitor, "height", 0) != 0)
                    newMonitor.height = JsonDom::getIntOr(monitor, "height", 200);
                else
                    newMonitor.height = 200;

                simdjson::dom::element visibleElem;
                if (JsonDom::hasKey(monitor, "visible") && !monitor["visible"].get(visibleElem) && !visibleElem.is_null())
                    newMonitor.visible = JsonDom::getBool(monitor, "visible").value_or(false);

                simdjson::dom::element isDiscreteElem;
                if (JsonDom::hasKey(monitor, "isDiscrete") && !monitor["isDiscrete"].get(isDiscreteElem) && !isDiscreteElem.is_null())
                    newMonitor.isDiscrete = JsonDom::getBool(monitor, "isDiscrete").value_or(false);

                simdjson::dom::element sliderMinElem;
                if (JsonDom::hasKey(monitor, "sliderMin") && !monitor["sliderMin"].get(sliderMinElem) && !sliderMinElem.is_null())
                    newMonitor.sliderMin = JsonDom::getDoubleOr(monitor, "sliderMin", 0);

                simdjson::dom::element sliderMaxElem;
                if (JsonDom::hasKey(monitor, "sliderMax") && !monitor["sliderMax"].get(sliderMaxElem) && !sliderMaxElem.is_null())
                    newMonitor.sliderMax = JsonDom::getDoubleOr(monitor, "sliderMax", 0);

                Render::monitors.emplace(newMonitor.id, newMonitor);
            }
        }

        Unzip::loadingState = "Finishing up!";

        Input::applyControls(Unzip::filePath + ".json");
        Parser::log("Loaded " + std::to_string(Scratch::sprites.size()) + " sprites.");
    }
}

void Parser::loadAdvancedProjectSettings(simdjson::dom::element json) {
    if (!JsonDom::hasKey(json, "comments")) return;

    simdjson::dom::element config;
    bool configFound = false;

    simdjson::dom::object commentsObj;
    if (json["comments"].get_object().get(commentsObj)) return;

    for (auto [id, data] : commentsObj) {
        std::string text = JsonDom::getString(data, "text").value_or("");
        std::size_t settingsFind = text.find("_twconfig_");
        if (settingsFind == std::string::npos) continue;

        std::size_t json_start = text.find('{');
        if (json_start == std::string::npos) continue;

        // Brace counting für JSON-Ende
        int braceCount = 0;
        std::size_t json_end = json_start;
        bool in_string = false;

        for (; json_end < text.size(); ++json_end) {
            char c = text[json_end];

            if (c == '"' && (json_end == 0 || text[json_end - 1] != '\\')) {
                in_string = !in_string;
            }

            if (!in_string) {
                if (c == '{') braceCount++;
                else if (c == '}') braceCount--;

                if (braceCount == 0) {
                    json_end++;
                    break;
                }
            }
        }

        if (braceCount != 0) continue;

        std::string json_str = text.substr(json_start, json_end - json_start);

        // Replace inifity with null, since the json cant handle infinity
        std::string cleaned_json = json_str;
        std::size_t inf_pos;
        while ((inf_pos = cleaned_json.find("Infinity")) != std::string::npos) {
            cleaned_json.replace(inf_pos, 8, "1e9");
        }

        if (JsonDom::parseNested(Unzip::nestedParser, cleaned_json, config)) {
            configFound = true;
            break;
        }
    }
    // set advanced project settings properties
    bool infClones = false;
    if (configFound && !config.is_null()) {

        Scratch::FPS = JsonDom::getIntOr(config, "framerate", 30);
        if (Scratch::FPS == 0) { // 0 FPS enables V-Sync
#if defined(RENDERER_SDL2)
            Scratch::FPS = 255; // SDL2's vsync will figure it out
#else
            Scratch::FPS = 60; // most platforms on other renderers are 60hz anyway
#endif
        }

        Scratch::turbo = JsonDom::getBoolOr(config, "turbo", false);
        Scratch::hqpen = JsonDom::getBoolOr(config, "hq", false);
        Scratch::projectWidth = JsonDom::getIntOr(config, "width", 480);
        Scratch::projectHeight = JsonDom::getIntOr(config, "height", 360);

        simdjson::dom::element runtimeOptions;
        if (!config["runtimeOptions"].get(runtimeOptions) && runtimeOptions.is_object()) {
            Scratch::fencing = JsonDom::getBoolOr(runtimeOptions, "fencing", true);
            Scratch::miscellaneousLimits = JsonDom::getBoolOr(runtimeOptions, "miscLimits", true);
            simdjson::dom::element maxClones;
            infClones = !runtimeOptions["maxClones"].get(maxClones) && !maxClones.is_null();
        }
    }

#ifdef RENDERER_CITRO2D
    if (Scratch::projectWidth == 400 && Scratch::projectHeight == 480)
        Render::renderMode = Render::BOTH_SCREENS;
    else if (Scratch::projectWidth == 320 && Scratch::projectHeight == 240)
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else {
        auto bottomScreen = Unzip::getSetting("bottomScreen");
        if (!bottomScreen.is_null() && bottomScreen.get_bool())
            Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
        else
            Render::renderMode = Render::TOP_SCREEN_ONLY;
    }
#elif defined(RENDERER_GL2D)
    auto bottomScreen = Unzip::getSetting("bottomScreen");
    if (!bottomScreen.is_null() && bottomScreen.get_bool())
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
        Render::renderMode = Render::TOP_SCREEN_ONLY;
#else
    Render::renderMode = Render::TOP_SCREEN_ONLY;
#endif

    auto accuratePen = Unzip::getSetting("accuratePen");
    if (!accuratePen.is_null())
        Scratch::accuratePen = accuratePen.get_bool();
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    else Scratch::accuratePen = true;
#else
    else Scratch::accuratePen = false;
#endif

    auto accurateCollision = Unzip::getSetting("accurateCollision");
    if (accurateCollision.is_null()) {
#ifdef __NDS__
        Scratch::accurateCollision = false;
#else
        Scratch::accurateCollision = true;
#endif
    } else Scratch::accurateCollision = accurateCollision.get_bool();

    auto debugVars = Unzip::getSetting("debugVars");
    if (!debugVars.is_null() && debugVars.get_bool())
        Scratch::debugVars = true;
    else Scratch::debugVars = false;

    auto withoutScreenRefreshLimit = Unzip::getSetting("warpTimer");
    if (!withoutScreenRefreshLimit.is_null() && withoutScreenRefreshLimit.is_bool())
        Scratch::warpTimer = withoutScreenRefreshLimit.get_bool();
    else Scratch::warpTimer = true;

    if (infClones) Scratch::maxClones = std::numeric_limits<int>::max();
    else Scratch::maxClones = 300;
}

void Parser::loadInputs(Block &block, Sprite *newSprite, std::string blockKey, simdjson::dom::element blockDatas, int indent) {
    simdjson::dom::element blockData;
    if (blockDatas[blockKey].get(blockData)) return;
    if (!JsonDom::hasKey(blockData, "inputs") || JsonDom::isEmptyObject(blockData["inputs"])) return;

    std::string indentStr(indent, '\t');

    const auto &removeBlock = [](Block *block) {
        if (Scratch::blocks.back() == block) Scratch::blocks.pop_back();
        else Scratch::blocks.erase(std::remove(Scratch::blocks.begin(), Scratch::blocks.end(), block), Scratch::blocks.end());
        delete block;
    };

    simdjson::dom::object inputsObj;
    if (blockData["inputs"].get_object().get(inputsObj)) return;

    for (auto [inputName, data] : inputsObj) {
        if (JsonDom::arraySize(data) < 2) continue;

        int type = JsonDom::getIntValue(JsonDom::arrayAt(data, 0));
        simdjson::dom::element inputValue = JsonDom::arrayAt(data, 1);

        if (type == 1) {
            if (inputValue.is_array() || block.opcode == "procedures_definition") {
                block.inputs[std::string(inputName)] = ParsedInput(Value::fromJson(inputValue));
                if (inputValue.is_array() && JsonDom::arraySize(inputValue) > 1) {
                    simdjson::dom::element valueElem = JsonDom::arrayAt(inputValue, 1);
                    std::string valueStr;
                    if (valueElem.is_string()) {
                        valueStr = JsonDom::getStringValue(valueElem).value_or("");
                    } else if (valueElem.is_number()) {
                        valueStr = std::to_string(JsonDom::getNumberValue(valueElem).value_or(0));
                    } else {
                        valueStr = JsonDom::toJsonString(valueElem);
                    }
                    Parser::log(indentStr + "\t" + std::string(inputName) + ": " + valueStr);
                }
            } else {
                if (!inputValue.is_null()) {
                    Parser::log(indentStr + "\t" + std::string(inputName) + ":");
                    Block *newBlock = loadBlock(newSprite, JsonDom::getStringValue(inputValue).value_or(""), blockDatas, &block, indent + 2);

                    // Check shadow block
                    const auto &it = getShadowBlocks().find(newBlock->opcode);
                    if (it != getShadowBlocks().end()) {
                        block.inputs[std::string(inputName)] = ParsedInput(Value(Scratch::getFieldValue(*newBlock, it->second)));
                        removeBlock(newBlock);
                        continue;
                    }

                    block.inputs[std::string(inputName)] = ParsedInput(newBlock);
                }
            }
        } else if (type == 2 || type == 3) {
            if (inputValue.is_array()) {
                if (JsonDom::arraySize(inputValue) < 3) continue;
                block.inputs[std::string(inputName)] = ParsedInput(JsonDom::getStringValue(JsonDom::arrayAt(inputValue, 2)).value_or(""));
                if (JsonDom::getIntValue(JsonDom::arrayAt(inputValue, 0)) == 13) block.inputs[std::string(inputName)].list = true;
                Parser::log(indentStr + "\t" + std::string(inputName) + ": var[" + JsonDom::getStringValue(JsonDom::arrayAt(inputValue, 1)).value_or("") + "]");
            } else {
                if (!inputValue.is_null()) {
                    Parser::log(indentStr + "\t" + std::string(inputName) + ":");
                    Block *newBlock = loadBlock(newSprite, JsonDom::getStringValue(inputValue).value_or(""), blockDatas, &block, indent + 2);

                    // Check shadow block
                    const auto &it = getShadowBlocks().find(newBlock->opcode);
                    if (it != getShadowBlocks().end()) {
                        block.inputs[std::string(inputName)] = ParsedInput(Value(Scratch::getFieldValue(*newBlock, it->second)));
                        removeBlock(newBlock);
                        continue;
                    }

                    // Constant folding :)
#define CHECK_NUM_CONSTANT_FOLDING(OPCODE, OPERATOR)                                                                                                                                 \
    if (newBlock->opcode == #OPCODE && newBlock->inputs["NUM1"].inputType == ParsedInput::InputType::VALUE && newBlock->inputs["NUM2"].inputType == ParsedInput::InputType::VALUE) { \
        block.inputs[std::string(inputName)] = ParsedInput(newBlock->inputs["NUM1"].value OPERATOR newBlock->inputs["NUM2"].value);                                                               \
        removeBlock(newBlock);                                                                                                                                                       \
        continue;                                                                                                                                                                    \
    }

                    CHECK_NUM_CONSTANT_FOLDING(operator_add, +)
                    CHECK_NUM_CONSTANT_FOLDING(operator_multiply, *)
                    CHECK_NUM_CONSTANT_FOLDING(operator_divide, /)
                    CHECK_NUM_CONSTANT_FOLDING(operator_subtract, -)
                    block.inputs[std::string(inputName)] = ParsedInput(newBlock);
                }
            }
        }
    }
}

void Parser::loadFields(Block &block, const std::string &blockKey, simdjson::dom::element blockDatas, int indent) {
    simdjson::dom::element blockData;
    if (blockDatas[blockKey].get(blockData)) return;
    if (!JsonDom::hasKey(blockData, "fields") || JsonDom::isEmptyObject(blockData["fields"])) return;

    std::string indentStr(indent, '\t');

    simdjson::dom::object fieldsObj;
    if (blockData["fields"].get_object().get(fieldsObj)) return;

    for (auto [name, field] : fieldsObj) {
        ParsedField parsedField;
        if (field.is_array() && JsonDom::arraySize(field) > 0) {
            parsedField.value = JsonDom::getStringValue(JsonDom::arrayAt(field, 0)).value_or("");

            if (JsonDom::arraySize(field) > 1) {
                simdjson::dom::element fieldId = JsonDom::arrayAt(field, 1);
                if (!fieldId.is_null()) {
                    parsedField.id = JsonDom::getStringValue(fieldId).value_or("");
                    Parser::log(indentStr + "\t" + std::string(name) + ": " + parsedField.value + " [" + parsedField.id + "]");
                } else {
                    Parser::log(indentStr + "\t" + std::string(name) + ": " + parsedField.value);
                }
            } else {
                Parser::log(indentStr + "\t" + std::string(name) + ": " + parsedField.value);
            }
        }
        block.fields[std::string(name)] = parsedField;
    }
}

static constexpr std::array<std::string_view, 12> builtInExtensions = {"music", "pen", "videoSensing", "text2speech", "translate", "makeymakey", "microbit", "ev3", "boost", "wedo2", "goDirect", "coreExtensions"};

bool Parser::loadExtensions(simdjson::dom::element json) {
    bool hasNativeExts = false;
#if defined(ENABLE_NATIVE_EXTENSIONS) || defined(ENABLE_CUSTOM_EXTENSIONS)
    const std::string folder = OS::getScratchFolderLocation() + "extensions/";

#ifdef __APPLE__
    constexpr const char *libraryExtension = ".dylib";
#else
    constexpr const char *libraryExtension = ".so";
#endif
    if (!JsonDom::hasKey(json, "extensions")) return false;
    simdjson::dom::array extensionsArray;
    if (json["extensions"].get_array().get(extensionsArray)) return false;
    for (simdjson::dom::element extensionElem : extensionsArray) {
        std::string targetID = JsonDom::getStringValue(extensionElem).value_or("");
        if (std::find(builtInExtensions.begin(), builtInExtensions.end(), targetID) != builtInExtensions.end()) continue;

#ifdef ENABLE_NATIVE_EXTENSIONS
        const std::string &nativePath = folder + targetID + libraryExtension;
        if (FileSystem::fileExists(nativePath)) {
            void *extensionHandle = dlopen(nativePath.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!extensionHandle) {
                Log::logError("Failed to load native extension, '" + targetID + "', dlerror: " + dlerror());
            } else {
                Log::log("Loaded native extension: " + targetID);
                hasNativeExts = true;
            }
            continue;
        }
#endif
#ifdef ENABLE_CUSTOM_EXTENSIONS
        std::unique_ptr<extensions::Extension> loadedExt = nullptr;
        std::ifstream in;

        const auto &tryPath = [&](std::string path) {
            in.open(path, std::ios::binary | std::ios::in);
            auto result = extensions::parseMetadata(in);
            if (result.has_value() && result.value()->id == targetID) {
                loadedExt = std::move(result.value());
            } else {
                if (!result.has_value()) Log::logWarning("Error while loading extension metadata: " + result.error());
                in.close();
                in.clear();
            }
        };

        const std::string romFSPath = OS::getRomFSLocation() + "extensions/" + targetID + ".see";
#ifdef USE_CMAKERC
        bool fromCmrc = false;

        const auto &fs = cmrc::romfs::get_filesystem();

        std::unique_ptr<std::istringstream> romfsStream = nullptr;
        if (fs.exists(romFSPath)) {
            const auto &romfsIn = fs.open(romFSPath);
            romfsStream = std::make_unique<std::istringstream>(std::string(romfsIn.begin(), romfsIn.end()));

            auto result = extensions::parseMetadata(*romfsStream);
            if (result.has_value() && result.value()->id == targetID) {
                loadedExt = std::move(result.value());
                fromCmrc = true;
            } else if (!result.has_value()) Log::logWarning("Error while loading extension metadata: " + result.error());
        }
#else
        if (FileSystem::fileExists(romFSPath)) {
            tryPath(romFSPath);
        }
#endif

        const std::string luaPath = folder + targetID + ".see";
        if (FileSystem::fileExists(luaPath) && !loadedExt) {
            tryPath(luaPath);
        }

        if (!loadedExt) {
            const auto &scanDirectory = [&](std::string path) {
                auto files = FileSystem::listDirectory(path);
                if (files.has_value()) {
                    for (const auto &file : files.value()) {
                        if (file.size() < 4) continue;
                        if (file.compare(file.size() - 4, 4, ".see") != 0) continue;

                        in.open(folder + file, std::ios::binary | std::ios::in);
                        auto result = extensions::parseMetadata(in);

                        if (result.has_value() && result.value()->id == targetID) {
                            loadedExt = std::move(result.value());
                            break;
                        }
                        if (!result.has_value()) Log::logWarning("Error while loading extension metadata: " + result.error());
                        in.close();
                        in.clear();
                    }
                }
            };

#if !defined(USE_CMAKERC) // I'm lazy, someone else can add this in the future.
            scanDirectory(OS::getRomFSLocation() + "extensions");
#endif
            if (!loadedExt) {
                scanDirectory(folder);
            }
        }

        if (loadedExt) {
#ifdef USE_CMAKERC
            if (fromCmrc) {
                extensions::loadLua(loadedExt.get(), *romfsStream);
            } else
#endif
                extensions::loadLua(loadedExt.get(), in);
            Scratch::extensions.push_back(std::move(loadedExt));
            in.close();
            Log::log("Successfully loaded Lua extension: " + targetID);
            continue;
        }

        Log::logError("Failed to find extension: " + targetID);
#endif
    }

#ifdef ENABLE_CUSTOM_EXTENSIONS
    extensions::registerHandlers();
#endif
#endif
    return hasNativeExts;
}

Block *Parser::loadBlock(Sprite *newSprite, const std::string id, simdjson::dom::element blockDatas, Block *parentBlock, int indent) {
    simdjson::dom::element firstBlockData;
    if (blockDatas[id].get(firstBlockData)) return parentBlock;
    if (!JsonDom::hasKey(firstBlockData, "opcode")) return parentBlock;

    Block *firstBlock = nullptr;
    Block *currentBlock = nullptr;
    std::string currentId = id;

    while (true) {
        simdjson::dom::element currentBlockData;
        if (blockDatas[currentId].get(currentBlockData) || !JsonDom::hasKey(currentBlockData, "opcode")) {
            if (currentBlock) {
                currentBlock->nextBlock = parentBlock;
            }
            break;
        }

        Block *newBlock = new Block();
        if (!firstBlock) firstBlock = newBlock;

        newBlock->opcode = JsonDom::getString(currentBlockData, "opcode").value_or("");

        std::string indentStr(indent, '\t');
        Parser::log(indentStr + newBlock->opcode);

        if (newBlock->opcode == "event_whenthisspriteclicked" || newBlock->opcode == "event_whenstageclicked") {
            newSprite->shouldDoSpriteClick = true;
        }

        loadInputs(*newBlock, newSprite, currentId, blockDatas, indent);
        loadFields(*newBlock, currentId, blockDatas, indent);

        if (BlockExecutor::getHandlers().count(newBlock->opcode) > 0) {
            newBlock->blockFunction = BlockExecutor::getHandlers()[newBlock->opcode];
        } else {
            Parser::log(indentStr + "No handler found for opcode: " + newBlock->opcode);
            newBlock->blockFunction = BlockExecutor::getHandlers()["coreExample_exampleOpcode"];
        }

        if (newBlock->opcode == "procedures_call") {
            simdjson::dom::element mutation;
            if (JsonDom::hasKey(currentBlockData, "mutation") && currentBlockData.is_object() &&
                !currentBlockData["mutation"].get(mutation) &&
                JsonDom::hasKey(mutation, "tagName") &&
                JsonDom::getString(mutation, "tagName") == "mutation") {

                std::string rawArgumentIds = JsonDom::getString(mutation, "argumentids").value_or("");
                newBlock->argumentIDs = JsonDom::parseStringArray(Unzip::nestedParser, rawArgumentIds);

                if (JsonDom::hasKey(mutation, "proccode")) {
                    Parser::log(indentStr + "\tproccode: " + JsonDom::getString(mutation, "proccode").value_or(""));
                }

                if (!newBlock->argumentIDs.empty()) {
                    Parser::log(indentStr + "\targuments: " + std::to_string(newBlock->argumentIDs.size()));
                }
                std::string procode = JsonDom::getString(mutation, "proccode").value_or("");

                if (procode == "\u200B\u200Blog\u200B\u200B %s") newBlock->blockFunction = BlockExecutor::getHandlers()["logs_log"];
                else if (procode == "\u200B\u200Bwarn\u200B\u200B %s") newBlock->blockFunction = BlockExecutor::getHandlers()["logs_warn"];
                else if (procode == "\u200B\u200Berror\u200B\u200B %s") newBlock->blockFunction = BlockExecutor::getHandlers()["logs_error"];
                else if (procode == "\u200B\u200Bopen\u200B\u200B %s .sb3") newBlock->blockFunction = BlockExecutor::getHandlers()["sceneManager_openSB3"];
                else if (procode == "\u200B\u200Bopen\u200B\u200B %s .sb3 with data %s") newBlock->blockFunction = BlockExecutor::getHandlers()["sceneManager_openSB3withData"];

                else {
                    if (newSprite->customHatBlock.count(procode) == 0) newSprite->customHatBlock[procode] = new Block();
                    newBlock->MyBlockDefinitionID = newSprite->customHatBlock[procode];
                }
            }
        }

        if (newBlock->opcode == "argument_reporter_boolean") {
            std::string name = Scratch::getFieldValue(*newBlock, "VALUE");
            if (name == "is Scratch Everywhere!?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isScratchEverywhere"];
            if (name == "is New 3DS?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isNew3DS"];
            if (name == "is DSi?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isDSi"];
        } else if (newBlock->opcode == "argument_reporter_string_number") {
            std::string name = Scratch::getFieldValue(*newBlock, "VALUE");
            if (name == "Scratch Everywhere! platform") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_platform"];
            if (name == "Scratch Everywhere! controller") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_controller"];

            if (name == "\u200B\u200Breceived data\u200B\u200B") newBlock->blockFunction = BlockExecutor::getHandlers()["sceneManager_receivedData"];
        }

        Scratch::blocks.push_back(newBlock);

        if (currentBlock) {
            currentBlock->nextBlock = newBlock;
        }
        currentBlock = newBlock;

        std::string nextId;
        bool hasNext = false;
        simdjson::dom::element nextElem;
        if (JsonDom::hasKey(currentBlockData, "next") && !currentBlockData["next"].get(nextElem) && !nextElem.is_null()) {
            nextId = JsonDom::getStringValue(nextElem).value_or("");
            hasNext = true;
        }

        simdjson::dom::element shadowElem;
        if (!JsonDom::hasKey(blockDatas[currentId], "shadow") || blockDatas[currentId]["shadow"].get(shadowElem) ||
            !JsonDom::getBoolOr(blockDatas[currentId], "shadow", false)) {
            newBlock->shadow = true;
        }

        if (hasNext) {
            currentId = nextId;
        } else {
            newBlock->isEndBlock = true;
            newBlock->nextBlock = parentBlock;
            break;
        }
    }
    return firstBlock;
}

void Parser::setSubstack(Block *startBlock, Block *stopBlock) {
    Block *current = startBlock;

    while (current != nullptr && current != stopBlock) {

        bool isIf = (current->opcode == "control_if" || current->opcode == "control_if_else");

        std::vector<std::string> substacks = {"SUBSTACK", "SUBSTACK2"};
        for (const std::string &stackName : substacks) {
            if (current->inputs.count(stackName) && current->inputs[stackName].block != nullptr) {
                Block *firstSubBlock = current->inputs[stackName].block;
                Block *sub = firstSubBlock;

                // go to end of substack
                while (sub->nextBlock != nullptr && sub->nextBlock != current) {
                    sub = sub->nextBlock;
                }

                if (isIf) {
                    // go to the block after the if block
                    if (sub->nextBlock == current) {
                        sub->nextBlock = current->nextBlock;
                        sub->isEndBlock = current->isEndBlock;
                    }
                    setSubstack(firstSubBlock, current->nextBlock);
                } else {
                    setSubstack(firstSubBlock, current);
                }
            }
        }

        if (!current->shadow) current->opcode.clear();
        if (current->isEndBlock) break;
        current = current->nextBlock;
    }
}
