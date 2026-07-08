#ifdef ENABLE_MOD_PENGUINMOD

// PenguinMod sound compat blocks used by example-scratch-mods/test_penguinmod.
// Opcodes: sound_play_at_seconds, sound_play_at_seconds_until_done, sound_playallsounds,
// sound_set_stop_fadeout_to, sound_isSoundPlaying, sound_getLength, sound_getEffectValue

#include "blockUtils.hpp"
#include "penguinmod.hpp"
#include <audiostack.hpp>
#include <cmath>
#include <runtime.hpp>
#include <unzip.hpp>

SCRATCH_BLOCK(sound, play_at_seconds) {
#ifdef ENABLE_AUDIO
    Value soundValue, startValue;
    if (!Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue) ||
        !Scratch::getInput(block, "VALUE", thread, sprite, startValue)) return BlockResult::REPEAT;

    std::string soundFullName;
    if (!penguinmod::resolveSoundMenu(sprite, soundValue, soundFullName)) return BlockResult::CONTINUE;

    Mixer::playSound(soundFullName, startValue.asDouble());
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, play_at_seconds_until_done) {
#ifdef ENABLE_AUDIO
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) {
        Value soundValue, startValue;
        if (!Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue) ||
            !Scratch::getInput(block, "VALUE", thread, sprite, startValue)) return BlockResult::REPEAT;

        if (!penguinmod::resolveSoundMenu(sprite, soundValue, state->name)) return BlockResult::CONTINUE;

        Mixer::playSound(state->name, startValue.asDouble());
        state->completedSteps = 1;
        return BlockResult::REPEAT;
    }

    if (!state->name.empty() && Mixer::isSoundPlaying(state->name)) return BlockResult::REPEAT;

    Mixer::setAutoClean(state->name, true);
    thread->eraseState(block);
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, playallsounds) {
#ifdef ENABLE_AUDIO
    for (const Sound &sound : sprite->sounds) {
        Mixer::playSound(sound.fullName, 0.0);
    }
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, set_stop_fadeout_to) {
#ifdef ENABLE_AUDIO
    Value fadeValue, soundValue;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, fadeValue) ||
        !Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

    std::string soundFullName;
    if (!penguinmod::resolveSoundMenu(sprite, soundValue, soundFullName)) return BlockResult::CONTINUE;

    Mixer::setStopFadeout(soundFullName, static_cast<float>(fadeValue.asDouble()));
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, isSoundPlaying) {
#ifdef ENABLE_AUDIO
    Value soundValue;
    if (!Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

    std::string soundFullName;
    if (!penguinmod::resolveSoundMenu(sprite, soundValue, soundFullName)) {
        *outValue = Value(false);
        return BlockResult::CONTINUE;
    }

    *outValue = Value(Mixer::isSoundPlaying(soundFullName));
#else
    *outValue = Value(false);
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, getLength) {
    Value soundValue;
    if (!Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

    std::string soundFullName;
    if (!penguinmod::resolveSoundMenu(sprite, soundValue, soundFullName)) {
        *outValue = Value(0.0);
        return BlockResult::CONTINUE;
    }

    if (const Sound *sound = penguinmod::findSoundByFullName(sprite, soundFullName)) {
        const double length = penguinmod::getSoundLengthSeconds(*sound);
        if (length > 0.0) {
            *outValue = Value(length);
            return BlockResult::CONTINUE;
        }
    }

#ifdef ENABLE_AUDIO
    Mixer::mutex.lock();
    const auto streamIt = Mixer::streams.find(soundFullName);
    if (streamIt != Mixer::streams.end() && streamIt->second != nullptr) {
        *outValue = Value(streamIt->second->getLengthSeconds());
        Mixer::mutex.unlock();
        return BlockResult::CONTINUE;
    }
    Mixer::mutex.unlock();
#endif

    *outValue = Value(0.0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, getEffectValue) {
    const std::string effect = Scratch::getFieldValue(*block, "EFFECT");

    if (effect == "PITCH") *outValue = Value(static_cast<double>(sprite->pitch));
    else if (effect == "PAN") *outValue = Value(static_cast<double>(sprite->pan));
    else *outValue = Value(0.0);

    return BlockResult::CONTINUE;
}

#endif
