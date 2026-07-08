#pragma once

#ifdef ENABLE_MOD_PENGUINMOD

#include <algorithm>
#include <blockExecutor.hpp>
#include <cmath>
#include <color.hpp>
#include <input.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>
#include <sprite.hpp>
#include <string>
#include <unordered_map>

namespace penguinmod {

inline std::string resolveSpeechFontPath(const std::string &name) {
    static const std::unordered_map<std::string, std::string> fonts = {
        {"", "gfx/ingame/fonts/NotoSans-Medium"},
        {"Sans Serif", "gfx/ingame/fonts/NotoSans-Medium"},
        {"Serif", "gfx/ingame/fonts/NotoSerif-Regular"},
        {"Handwriting", "gfx/ingame/fonts/Handlee-Regular"},
        {"Marker", "gfx/ingame/fonts/Knewave-Regular"},
        {"Curly", "gfx/ingame/fonts/Griffy-Regular"},
        {"Pixel", "gfx/ingame/fonts/Grand9KPixel"},
        {"Archivo", "gfx/ingame/fonts/Archivo-Regular"},
        {"Monospace", "gfx/ingame/fonts/Monospace"},
        {"Press Start 2P", "gfx/ingame/fonts/PressStart2P"},
        {"Bad Comic", "gfx/ingame/fonts/BadComic-Regular"},
    };

    const auto it = fonts.find(name);
    if (it != fonts.end()) return it->second;
    return "gfx/ingame/fonts/NotoSans-Medium";
}

inline void ensureCostumeImageLoaded(Sprite *sprite, int costumeIndex) {
    if (costumeIndex < 0 || costumeIndex >= static_cast<int>(sprite->costumes.size())) return;

    const Costume &costume = sprite->costumes[costumeIndex];
    if (Scratch::costumeImages.find(costume.fullName) != Scratch::costumeImages.end()) return;

    const int savedCostume = sprite->currentCostume;
    sprite->currentCostume = costumeIndex;
    Scratch::loadCurrentCostumeImage(sprite);
    sprite->currentCostume = savedCostume;
}

inline int colorValueToArgb(const Value &colorValue) {
    if (colorValue.isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(colorValue.asColor());
        const int r = static_cast<unsigned char>(rgb.r);
        const int g = static_cast<unsigned char>(rgb.g);
        const int b = static_cast<unsigned char>(rgb.b);
        return (r << 24) | (g << 16) | (b << 8) | 0xFF;
    }

    const std::string hex = colorValue.asString();
    if (hex.size() >= 7 && hex[0] == '#') {
        const int r = std::stoi(hex.substr(1, 2), nullptr, 16);
        const int g = std::stoi(hex.substr(3, 2), nullptr, 16);
        const int b = std::stoi(hex.substr(5, 2), nullptr, 16);
        return (r << 24) | (g << 16) | (b << 8) | 0xFF;
    }

    return 0;
}

inline Sprite *findSpriteByName(const std::string &name) {
    for (Sprite *s : Scratch::sprites) {
        if (s->isClone) continue;
        if (s->name == name) return s;
    }
    return nullptr;
}

inline Sprite *resolveSpriteTarget(const std::string &option, Sprite *self) {
    if (option == "_myself_") return self;
    if (option == "_stage_") return Scratch::stageSprite;
    return findSpriteByName(option);
}

inline int spriteListIndex(Sprite *sprite) {
    for (size_t i = 0; i < Scratch::sprites.size(); i++) {
        if (Scratch::sprites[i] == sprite) return static_cast<int>(i);
    }
    return -1;
}

inline void reindexLayers(int from, int to) {
    const int start = std::min(from, to);
    const int end = std::max(from, to);
    for (int i = start; i <= end; i++) {
        Scratch::sprites[i]->layer = static_cast<int>(Scratch::sprites.size() - 1) - i;
    }
    BlockExecutor::sortSprites = true;
}

inline void shiftSpriteLayers(Sprite *sprite, int shift) {
    if (sprite->isStage || shift == 0) return;

    const int currentIndex = static_cast<int>(Scratch::sprites.size() - 1) - sprite->layer;
    const int maxIndex = static_cast<int>(Scratch::sprites.size()) - 2;
    const int targetIndex = std::clamp(currentIndex - shift, 0, maxIndex);
    if (targetIndex == currentIndex) return;

    if (targetIndex < currentIndex) {
        std::rotate(Scratch::sprites.begin() + targetIndex, Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1);
    } else {
        std::rotate(Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1, Scratch::sprites.begin() + targetIndex + 1);
    }

    reindexLayers(std::min(currentIndex, targetIndex), std::max(currentIndex, targetIndex));
}

inline void placeSpriteAtLayer(Sprite *sprite, int targetLayer) {
    if (sprite->isStage) return;
    const int currentLayer = sprite->layer;
    shiftSpriteLayers(sprite, targetLayer - currentLayer);
}

inline void goTargetLayer(Sprite *sprite, Sprite *other, bool infront) {
    if (sprite->isStage || other == nullptr) return;

    const int otherIndex = spriteListIndex(other);
    if (otherIndex < 0) return;

    const int currentIndex = spriteListIndex(sprite);
    if (currentIndex < 0) return;

    int targetIndex = otherIndex;
    if (infront && targetIndex > 0) targetIndex--;

    if (targetIndex == currentIndex) return;

    Scratch::sprites.erase(Scratch::sprites.begin() + currentIndex);
    if (currentIndex < targetIndex) targetIndex--;

    Scratch::sprites.insert(Scratch::sprites.begin() + targetIndex, sprite);
    reindexLayers(0, static_cast<int>(Scratch::sprites.size()) - 1);
}

inline void bounceAwayFromPoint(Sprite *sprite, double pointX, double pointY) {
    const Costume &costume = sprite->costumes[sprite->currentCostume];
    const double scale = (sprite->size / 100.0) / costume.bitmapResolution;
    const double spriteHalfWidth = (sprite->spriteWidth * scale) / 2.0;
    const double spriteHalfHeight = (sprite->spriteHeight * scale) / 2.0;

    const double left = sprite->xPosition - spriteHalfWidth;
    const double right = sprite->xPosition + spriteHalfWidth;
    const double top = sprite->yPosition + spriteHalfHeight;
    const double bottom = sprite->yPosition - spriteHalfHeight;

    const double distLeft = std::abs(pointX - left);
    const double distRight = std::abs(pointX - right);
    const double distTop = std::abs(pointY - top);
    const double distBottom = std::abs(pointY - bottom);

    std::string nearestEdge;
    double minDist = distLeft;
    nearestEdge = "left";
    if (distTop < minDist) {
        minDist = distTop;
        nearestEdge = "top";
    }
    if (distRight < minDist) {
        minDist = distRight;
        nearestEdge = "right";
    }
    if (distBottom < minDist) {
        nearestEdge = "bottom";
    }

    const double radians = Math::degreesToRadians(90.0 - sprite->rotation);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);

    if (nearestEdge == "left") dx = std::max(0.2, std::fabs(dx));
    else if (nearestEdge == "right") dx = -std::max(0.2, std::fabs(dx));
    else if (nearestEdge == "top") dy = std::max(0.2, std::fabs(dy));
    else if (nearestEdge == "bottom") dy = -std::max(0.2, std::fabs(dy));

    Scratch::setDirection(sprite, Math::radiansToDegrees(atan2(dy, dx)) + 90.0);
    Scratch::fenceSpriteWithinBounds(sprite);
}

inline int resolveCostumeIndex(Sprite *sprite, const std::string &costumeName) {
    if (costumeName == "next costume" || costumeName == "next backdrop") {
        return (sprite->currentCostume + 1) % static_cast<int>(sprite->costumes.size());
    }
    if (costumeName == "previous costume" || costumeName == "previous backdrop") {
        int index = sprite->currentCostume - 1;
        if (index < 0) index = static_cast<int>(sprite->costumes.size()) - 1;
        return index;
    }
    if (costumeName == "random costume" || costumeName == "random backdrop") {
        if (sprite->costumes.size() <= 1) return sprite->currentCostume;
        int index;
        do {
            index = rand() % static_cast<int>(sprite->costumes.size());
        } while (index == sprite->currentCostume);
        return index;
    }

    for (size_t i = 0; i < sprite->costumes.size(); i++) {
        if (sprite->costumes[i].name == costumeName) return static_cast<int>(i);
    }

    if (costumeName.size() > 0 && std::isdigit(static_cast<unsigned char>(costumeName[0]))) {
        const int index = std::stoi(costumeName) - 1;
        if (index >= 0 && index < static_cast<int>(sprite->costumes.size())) return index;
    }

    return -1;
}

inline bool resolveSoundMenu(Sprite *sprite, const Value &soundValue, std::string &outFullName) {
    if (sprite->sounds.empty()) return false;

    if (soundValue.isString()) {
        for (const Sound &sound : sprite->sounds) {
            if (sound.name == soundValue.asString()) {
                outFullName = sound.fullName;
                return true;
            }
        }
    }

    if (soundValue.isNaN() || !soundValue.isNumeric()) return false;

    const double index = std::trunc(soundValue.asDouble());
    const double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
    outFullName = sprite->sounds[static_cast<size_t>(soundIndex)].fullName;
    return true;
}

inline const Sound *findSoundByFullName(Sprite *sprite, const std::string &fullName) {
    for (const Sound &sound : sprite->sounds) {
        if (sound.fullName == fullName) return &sound;
    }
    return nullptr;
}

inline double getSoundLengthSeconds(const Sound &sound) {
    if (sound.sampleRate > 0 && sound.sampleCount > 0) {
        return sound.sampleCount / static_cast<double>(sound.sampleRate);
    }
    return 0.0;
}

} // namespace penguinmod

#endif
