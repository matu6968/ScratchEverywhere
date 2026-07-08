#ifdef ENABLE_MOD_PENGUINMOD

// PenguinMod control compat blocks used by example-scratch-mods/test_penguinmod.
// Opcodes: control_expandableIf, control_waitsecondsoruntil, control_backToGreenFlag

#include "blockUtils.hpp"
#include <runtime.hpp>

SCRATCH_BLOCK(control, expandableIf) {
    int branch = 1;
    while (block->inputs.find("BOOL" + std::to_string(branch)) != block->inputs.end()) {
        Value conditionValue;
        if (!Scratch::getInput(block, "BOOL" + std::to_string(branch), thread, sprite, conditionValue)) return BlockResult::REPEAT;

        if (conditionValue.asBoolean()) {
            Block *substack = block->inputs["SUBSTACK" + std::to_string(branch)].block;
            if (substack != nullptr) {
                thread->nextBlock = substack;
                return BlockResult::CONTINUE_IMMEDIATELY;
            }
            return BlockResult::CONTINUE;
        }

        branch++;
    }

    const std::string elseInput = "SUBSTACK" + std::to_string(branch);
    if (block->inputs.find(elseInput) != block->inputs.end()) {
        Block *substack = block->inputs[elseInput].block;
        if (substack != nullptr) {
            thread->nextBlock = substack;
            return BlockResult::CONTINUE_IMMEDIATELY;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, waitsecondsoruntil) {
    BlockState *state = thread->getState(block);

    Value condition;
    if (!Scratch::getInput(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;
    if (condition.asBoolean()) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    if (state->completedSteps == 0) {
        Value duration;
        if (!Scratch::getInput(block, "DURATION", thread, sprite, duration)) return BlockResult::REPEAT;

        state->waitDuration = duration.asDouble() * 1000;
        state->waitTimer.start();
        state->completedSteps = 1;
    }

    if (state->waitTimer.hasElapsed(state->waitDuration)) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    Scratch::resetInput(block, "");
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(control, backToGreenFlag) {
    BlockExecutor::greenFlagClicked = true;
    return BlockResult::RETURN;
}

#endif
