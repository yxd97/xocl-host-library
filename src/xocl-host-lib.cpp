#ifndef XOCL_HOSTLIB
#define XOCL_HOSTLIB

#include <vector>
#include <unordered_map>


#include "xocl-host-lib.hpp"

namespace xhl {

/**
 * @brief
 *
 * @return ExecMode
 */
ExecMode check_execution_mode() {
    if(xcl::is_emulation()) {
        if(xcl::is_hw_emulation())
            return ExecMode::HW_EMU;
        return ExecMode::SW_EMU;
    }
    return ExecMode::HW;
}

};

#endif //XOCL_HOSTLIB