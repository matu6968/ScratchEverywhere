#ifdef ENABLE_MOD_PENGUINMOD

// PenguinMod motion compat blocks used by example-scratch-mods/test_penguinmod.
// Opcodes: motion_changebyxy, motion_pointtowardsxy, motion_ifonspritebounce, motion_move_sprite_to_scene_side

#include "blockUtils.hpp"
#include "penguinmod.hpp"
#include <cmath>
#include <input.hpp>
#include <math.hpp>

SCRATCH_BLOCK(motion, changebyxy) {
    Value dx, dy;
    if (!Scratch::getInput(block, "DX", thread, sprite, dx) ||
        !Scratch::getInput(block, "DY", thread, sprite, dy)) return BlockResult::REPEAT;

    Scratch::gotoXY(sprite, sprite->xPosition + dx.asDouble(), sprite->yPosition + dy.asDouble());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, pointtowardsxy) {
    Value xValue, yValue;
    if (!Scratch::getInput(block, "X", thread, sprite, xValue) ||
        !Scratch::getInput(block, "Y", thread, sprite, yValue)) return BlockResult::REPEAT;

    const double dx = xValue.asDouble() - sprite->xPosition;
    const double dy = yValue.asDouble() - sprite->yPosition;
    Scratch::setDirection(sprite, 90.0 - Math::radiansToDegrees(atan2(dy, dx)));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, ifonspritebounce) {
    Value spriteValue;
    if (!Scratch::getInput(block, "SPRITE", thread, sprite, spriteValue)) return BlockResult::REPEAT;

    const std::string targetName = spriteValue.asString();
    if (targetName == "_mouse_") {
        if (!Scratch::isColliding("mouse", sprite)) return BlockResult::CONTINUE;
        penguinmod::bounceAwayFromPoint(sprite, Input::mousePointer.x, Input::mousePointer.y);
        return BlockResult::CONTINUE;
    }

    if (targetName == "_random_") {
        const double x = (rand() % Scratch::projectWidth) - Scratch::projectWidth / 2.0;
        const double y = (rand() % Scratch::projectHeight) - Scratch::projectHeight / 2.0;
        penguinmod::bounceAwayFromPoint(sprite, x, y);
        return BlockResult::CONTINUE;
    }

    Sprite *target = penguinmod::findSpriteByName(targetName);
    if (target == nullptr || !Scratch::isColliding("sprite", sprite, target)) return BlockResult::CONTINUE;
    penguinmod::bounceAwayFromPoint(sprite, target->xPosition, target->yPosition);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, move_sprite_to_scene_side) {
    const std::string alignment = Scratch::getFieldValue(*block, "ALIGNMENT");
    const double halfWidth = Scratch::projectWidth / 2.0;
    const double halfHeight = Scratch::projectHeight / 2.0;

    double x = sprite->xPosition;
    double y = sprite->yPosition;

    if (alignment == "top") y = halfHeight;
    else if (alignment == "bottom") y = -halfHeight;
    else if (alignment == "left") x = -halfWidth;
    else if (alignment == "right") x = halfWidth;
    else if (alignment == "middle") {
        x = 0;
        y = 0;
    } else if (alignment == "top-left") {
        x = -halfWidth;
        y = halfHeight;
    } else if (alignment == "top-right") {
        x = halfWidth;
        y = halfHeight;
    } else if (alignment == "bottom-left") {
        x = -halfWidth;
        y = -halfHeight;
    } else if (alignment == "bottom-right") {
        x = halfWidth;
        y = -halfHeight;
    }

    Scratch::gotoXY(sprite, x, y);
    Scratch::fenceSpriteWithinBounds(sprite);
    return BlockResult::CONTINUE;
}

#endif
