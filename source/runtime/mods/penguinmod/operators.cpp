#ifdef ENABLE_MOD_PENGUINMOD

// Initial PenguinMod operator compat subset (not full parity).
// Opcodes: operator_indexOfTextInText, operator_lerpFunc, operator_trueBoolean, operator_falseBoolean

#include "blockUtils.hpp"
#include <string>

SCRATCH_BLOCK(operator, indexOfTextInText) {
    Value text1, text2;
    if (!Scratch::getInput(block, "TEXT1", thread, sprite, text1) ||
        !Scratch::getInput(block, "TEXT2", thread, sprite, text2)) return BlockResult::REPEAT;

    const std::string lookFor = text1.asString();
    const std::string searchIn = text2.asString();
    size_t index = 0;
    const size_t found = searchIn.find(lookFor);
    if (found != std::string::npos) index = found + 1;

    *outValue = Value(static_cast<double>(index));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, lerpFunc) {
    Value one, two, amount;
    if (!Scratch::getInput(block, "ONE", thread, sprite, one) ||
        !Scratch::getInput(block, "TWO", thread, sprite, two) ||
        !Scratch::getInput(block, "AMOUNT", thread, sprite, amount)) return BlockResult::REPEAT;

    const double start = one.asDouble();
    const double end = two.asDouble();
    const double t = amount.asDouble();
    *outValue = Value(((end - start) * t) + start);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, trueBoolean) {
    *outValue = Value(true);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, falseBoolean) {
    *outValue = Value(false);
    return BlockResult::CONTINUE;
}

#endif
