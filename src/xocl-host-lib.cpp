#ifndef XOCL_HOSTLIB
#define XOCL_HOSTLIB

#include <vector>
#include <unordered_map>
#include "device.hpp"
#include "buffer.hpp"

#include "xocl-host-lib.hpp"

namespace xhl {

void nb_sync_data_htod(xhl::Device* d, xhl::IBuffer* b) {
    d->_errflag = d->command_q.enqueueMigrateMemObjects(
        {b->buffer()},
        0
    );
    if (d->_errflag != CL_SUCCESS) {
        throw std::runtime_error(
            "Failed to migrate data for buffer to device (code:"
            + std::to_string(d->_errflag) + ")"
        );
    }
}
void nb_sync_data_dtoh(xhl::Device* d, xhl::IBuffer* b) {
    d->_errflag = d->command_q.enqueueMigrateMemObjects(
        {b->buffer()},
        1
    );
    if (d->_errflag != CL_SUCCESS) {
        throw std::runtime_error(
            "Failed to migrate data for buffer to host (code:"
            + std::to_string(d->_errflag) + ")"
        );
    }
}
void sync_data_htod(xhl::Device* d, xhl::IBuffer* b) {
    nb_sync_data_htod(d, b);
    d->command_q.finish();
}
void sync_data_dtoh(xhl::Device* d, xhl::IBuffer* b) {
    nb_sync_data_dtoh(d, b);
    d->command_q.finish();
}

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