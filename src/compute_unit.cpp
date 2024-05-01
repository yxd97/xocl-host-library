#include "compute_unit.hpp"

namespace xhl {

void ComputeUnit::bind (xhl::Device *d) {
    this->cu_device = d;
    // create CL kernel in compute unit
    cl_int errflag = 0;
    this->clkernel = cl::Kernel(
        this->cu_device->program, this->signature.name.c_str(), &errflag
    );
    if (errflag != CL_SUCCESS) {
        throw std::runtime_error(
            "[ERROR]: Failed to create CL Kernel, exit! (code:"
            + std::to_string(errflag) + ")"
        );
    }
}

} // namespace xhl