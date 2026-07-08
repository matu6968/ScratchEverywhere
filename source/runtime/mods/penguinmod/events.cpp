#ifdef ENABLE_MOD_PENGUINMOD

// PenguinMod event compat hats. Scheduling for the active hats is handled by
// BlockExecutor so these handlers only need to make the opcodes known.

#include "blockUtils.hpp"

SCRATCH_BLOCK(event, always) {
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, whenanything) {
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, whenkeyhit) {
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, whenmousescrolled) {
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, whenstopclicked) {
    return BlockResult::CONTINUE;
}

#endif
