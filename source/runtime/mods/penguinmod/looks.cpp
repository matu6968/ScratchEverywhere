#ifdef ENABLE_MOD_PENGUINMOD

// PenguinMod looks compat blocks used by example-scratch-mods/test_penguinmod.
// Opcodes: looks_getEffectValue, looks_getSpriteVisible, looks_getOtherSpriteVisible,
// looks_changeVisibilityOfSpriteShow/Hide, looks_stoptalking, looks_sayWidth/Height,
// looks_setStretch/changeStretch, looks_stretchGetX/Y, looks_layersGetLayer/SetLayer,
// looks_goTargetLayer, looks_getinputofcostume, looks_setFont/setColor/setShape,
// looks_setTintColor, looks_tintColor

#include "blockUtils.hpp"
#include "penguinmod.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <render.hpp>
#include <speech_manager.hpp>
#include <string>

#ifdef ENABLE_MOD_PENGUINMOD
SCRATCH_SHADOW_BLOCK(looks_getinput_menu, INPUT)
#endif

static void markSpriteRenderDirty(Sprite *sprite) {
    sprite->renderInfo.oldSize++;
    if (sprite->visible) Scratch::forceRedraw = true;
}

SCRATCH_BLOCK(looks, getEffectValue) {
    std::string effect = Scratch::getFieldValue(*block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::tolower);

    if (effect == "ghost") *outValue = Value(static_cast<double>(sprite->ghostEffect));
    else if (effect == "brightness") *outValue = Value(static_cast<double>(sprite->brightnessEffect));
    else if (effect == "color") *outValue = Value(static_cast<double>(sprite->colorEffect));
    else *outValue = Value(0.0);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, getSpriteVisible) {
    *outValue = Value(sprite->visible);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, getOtherSpriteVisible) {
    Value option;
    if (!Scratch::getInput(block, "VISIBLE_OPTION", thread, sprite, option)) return BlockResult::REPEAT;

    Sprite *target = penguinmod::resolveSpriteTarget(option.asString(), sprite);
    *outValue = Value(target != nullptr && target->visible);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changeVisibilityOfSpriteShow) {
    Value option;
    if (!Scratch::getInput(block, "VISIBLE_OPTION", thread, sprite, option)) return BlockResult::REPEAT;

    Sprite *target = penguinmod::resolveSpriteTarget(option.asString(), sprite);
    if (target != nullptr && !target->visible) {
        Scratch::loadCurrentCostumeImage(target);
        target->visible = true;
        Scratch::forceRedraw = true;
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changeVisibilityOfSpriteHide) {
    Value option;
    if (!Scratch::getInput(block, "VISIBLE_OPTION", thread, sprite, option)) return BlockResult::REPEAT;

    Sprite *target = penguinmod::resolveSpriteTarget(option.asString(), sprite);
    if (target != nullptr && !target->isStage) {
        target->visible = false;
        Scratch::forceRedraw = true;
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, stoptalking) {
    if (SpeechManager *speechManager = Render::getSpeechManager()) {
        speechManager->showSpeech(sprite, "", 0.01, "say");
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, sayWidth) {
    SpeechManager *speechManager = Render::getSpeechManager();
    if (speechManager == nullptr || speechManager->getSpeechText(sprite).empty()) {
        *outValue = Value(0.0);
        return BlockResult::CONTINUE;
    }

    const auto size = speechManager->getSpeechSize(sprite);
    *outValue = Value(static_cast<double>(size[0]));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, sayHeight) {
    SpeechManager *speechManager = Render::getSpeechManager();
    if (speechManager == nullptr || speechManager->getSpeechText(sprite).empty()) {
        *outValue = Value(0.0);
        return BlockResult::CONTINUE;
    }

    const auto size = speechManager->getSpeechSize(sprite);
    *outValue = Value(static_cast<double>(size[1]));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setStretch) {
    Value x, y;
    if (!Scratch::getInput(block, "X", thread, sprite, x) ||
        !Scratch::getInput(block, "Y", thread, sprite, y)) return BlockResult::REPEAT;

    sprite->stretchX = static_cast<float>(x.asDouble());
    sprite->stretchY = static_cast<float>(y.asDouble());
    markSpriteRenderDirty(sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changeStretch) {
    Value x, y;
    if (!Scratch::getInput(block, "X", thread, sprite, x) ||
        !Scratch::getInput(block, "Y", thread, sprite, y)) return BlockResult::REPEAT;

    sprite->stretchX += static_cast<float>(x.asDouble());
    sprite->stretchY += static_cast<float>(y.asDouble());
    markSpriteRenderDirty(sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, stretchGetX) {
    *outValue = Value(static_cast<double>(sprite->stretchX));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, stretchGetY) {
    *outValue = Value(static_cast<double>(sprite->stretchY));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, layersGetLayer) {
    *outValue = Value(static_cast<double>(sprite->layer));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, layersSetLayer) {
    Value layer;
    if (!Scratch::getInput(block, "NUM", thread, sprite, layer)) return BlockResult::REPEAT;
    penguinmod::placeSpriteAtLayer(sprite, static_cast<int>(std::round(layer.asDouble())));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, goTargetLayer) {
    Value option;
    if (!Scratch::getInput(block, "VISIBLE_OPTION", thread, sprite, option)) return BlockResult::REPEAT;

    Sprite *target = penguinmod::resolveSpriteTarget(option.asString(), sprite);
    const std::string direction = Scratch::getFieldValue(*block, "FORWARD_BACKWARD");
    penguinmod::goTargetLayer(sprite, target, direction == "infront");
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, getinputofcostume) {
    Value costumeInput, propertyInput;
    if (!Scratch::getInput(block, "COSTUME", thread, sprite, costumeInput) ||
        !Scratch::getInput(block, "INPUT", thread, sprite, propertyInput)) return BlockResult::REPEAT;

    const int costumeIndex = penguinmod::resolveCostumeIndex(sprite, costumeInput.asString());
    const std::string property = propertyInput.asString();

    if (costumeIndex < 0 || costumeIndex >= static_cast<int>(sprite->costumes.size())) {
        if (property == "width" || property == "height" || property == "rotation center x" || property == "rotation center y") {
            *outValue = Value(0.0);
        } else {
            *outValue = Value("");
        }
        return BlockResult::CONTINUE;
    }

    penguinmod::ensureCostumeImageLoaded(sprite, costumeIndex);

    const Costume &costume = sprite->costumes[costumeIndex];
    int costumeWidth = 0;
    int costumeHeight = 0;
    const auto imageIt = Scratch::costumeImages.find(costume.fullName);
    if (imageIt != Scratch::costumeImages.end()) {
        costumeWidth = imageIt->second->getWidth();
        costumeHeight = imageIt->second->getHeight();
    }

    if (property == "width") *outValue = Value(static_cast<double>(costumeWidth));
    else if (property == "height") *outValue = Value(static_cast<double>(costumeHeight));
    else if (property == "rotation center x") *outValue = Value(static_cast<double>(costume.rotationCenterX));
    else if (property == "rotation center y") *outValue = Value(static_cast<double>(costume.rotationCenterY));
    else if (property == "drawing mode") *outValue = Value(std::string(costume.dataFormat == "svg" ? "Vector" : "Bitmap"));
    else *outValue = Value("");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setFont) {
    Value fontName, fontSize;
    if (!Scratch::getInput(block, "font", thread, sprite, fontName) ||
        !Scratch::getInput(block, "size", thread, sprite, fontSize)) return BlockResult::REPEAT;

    sprite->speechFontPath = penguinmod::resolveSpeechFontPath(fontName.asString());
    sprite->speechFontSize = static_cast<int>(std::round(fontSize.asDouble()));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setColor) {
    Value color;
    if (!Scratch::getInput(block, "color", thread, sprite, color)) return BlockResult::REPEAT;

    sprite->speechTextColor = penguinmod::colorValueToArgb(color);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setShape) {
    Value value;
    if (!Scratch::getInput(block, "color", thread, sprite, value)) return BlockResult::REPEAT;

    const std::string prop = Scratch::getFieldValue(*block, "prop");
    const int intValue = static_cast<int>(std::round(value.asDouble()));

    if (prop == "MIN_WIDTH" || prop == "MAX_LINE_WIDTH") {
        sprite->speechBubbleMaxWidth = std::max(1, intValue);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setTintColor) {
    Value color;
    if (!Scratch::getInput(block, "color", thread, sprite, color)) return BlockResult::REPEAT;

    if (color.isColor()) {
        const ColorRGBA rgb = CSBT2RGBA(color.asColor());
        const unsigned char r = static_cast<unsigned char>(rgb.r);
        const unsigned char g = static_cast<unsigned char>(rgb.g);
        const unsigned char b = static_cast<unsigned char>(rgb.b);
        char hex[8];
        std::snprintf(hex, sizeof(hex), "#%02x%02x%02x", r, g, b);
        sprite->speechTintColor = hex;
    } else {
        sprite->speechTintColor = color.asString();
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, tintColor) {
    *outValue = Value(sprite->speechTintColor);
    return BlockResult::CONTINUE;
}

#endif
