#ifndef COMPUTE_UNIT_HPP
#define COMPUTE_UNIT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <stdexcept>

#include "xcl2.hpp"
#include "device.hpp"
#include "buffer.hpp"
#include "xocl-host-lib.hpp"

namespace xhl {
class ComputeUnit {
private:

template<typename T>
void __set_arg(
    const int arg_index,
    const T&arg_val
) {
    cl_int errflag = 0;
    if constexpr (std::is_base_of_v<IBuffer, T>) {
        const IBuffer* ptr = &arg_val;
        errflag = this->clkernel.setArg(arg_index, ptr->buffer());
    }
    else {
        errflag = this->clkernel.setArg(arg_index, arg_val);
    }
    if (errflag != CL_SUCCESS) {
        throw std::runtime_error(
            "[ERROR]: Failed to setArg in CL Kernel, exit! (code:"
            + std::to_string(errflag) + ")"
        );
    }
}

// reference: https://stackoverflow.com/a/71148253
template <typename ...Ts, size_t ...Is>
void __invoke_at_impl(
    std::tuple<Ts...>& tpl, std::index_sequence<Is...>, size_t idx
) {
    ((void)(Is == idx && (this->__set_arg(idx, std::get<Is>(tpl)), true)), ...);
}

template <typename ...Ts>
void __invoke_at(std::tuple<Ts...>& tpl, size_t idx) {
    __invoke_at_impl(tpl, std::make_index_sequence<sizeof...(Ts)>{}, idx);
}

public:
xhl::Device *cu_device;
struct xhl::KernelSignature signature;
cl::Kernel clkernel;

ComputeUnit() = delete; // don't provide default constructor

/**
 * @brief constructor instantiate the xhl::kernel into the ComputeUnit
 *
 * @param Kernel class under xhl namespace
 */
ComputeUnit(struct xhl::KernelSignature ks) : cu_device(nullptr) { // add xhl::kernel and assign it to computeunit.
    this->signature = ks;
}

/**
 * @brief bind the device with the computeunit and create the cl::Kernel, which is the computeunit in our define
 *
 * @param Device class under xhl::runtime namespace
 *
 * @exception Failed to create CL Kernel
 */
void bind (xhl::Device *d);

/**
 * @brief set argument in clkernel
 *
 * @param argument name
 * @param argument content in any type corresponding to the name
 *
 * @exception Failed to set argument
 * @exception Failed to match any argument name in map
 */
template<typename T>
void set_arg(
    const std::string& arg_name,
    const T&arg_val
) {
    cl_int errflag = 0;

    // use iterator to find the match between the input argument name
    // and the argument map in kernel signature
    // argmap maps from argument name to argument type
    std::map<std::string, std::string>::iterator it;
    int i = 0;
    for (it = this->signature.argmap.begin(); it != this->signature.argmap.end() ; ++it) {
        if (it -> first == arg_name) {
            set_arg(i, arg_val);
        }
        i++;
    }
    // check if match
    if (i == this->signature.argmap.size()) {
        throw std::runtime_error(
            "[ERROR]: input argument name doesn't match with the argument map"
            " (map from argument name to argument type), exit!\n"
        );
    }
}

/**
 * @brief launch, start to run the computeunit
 *
 * @param ... arguments of the kernel signature
 */
template <typename... Ts>
void launch (Ts ... ts) {
    if (sizeof...(Ts) < this->signature.argmap.size()) {
        throw std::runtime_error("Too few arguments supplied to compute unit launch");
    }
    if (sizeof...(Ts) > this->signature.argmap.size()) {
        throw std::runtime_error("Too many arguments supplied to compute unit launch");
    }

    auto args = std::make_tuple(ts...);
    auto ite = this->signature.argmap.begin();
    for (int i = 0; i < this->signature.argmap.size(); i++, ite++) {
        this->__invoke_at(args, i);
    }
    this->cu_device->command_q.enqueueTask(this->clkernel);
}

}; // class ComputeUnit
} // namespace xhl


#endif // COMPUTE_UNIT_HPP