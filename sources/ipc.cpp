#include "ipc.h"

extern "C" {
    bool verifyExecCaps(const uint8_t execFunFlags) {
        const uint8_t allExecFunFlags =
            ExecFunFlags::ADD |
            ExecFunFlags::SUB |
            ExecFunFlags::MULT |
            ExecFunFlags::DIV |
            ExecFunFlags::CONCAT |
            ExecFunFlags::FIND_START;
        return (execFunFlags & ~allExecFunFlags) == 0 && execFunFlags > 0;
    }
}
