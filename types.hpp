#ifndef XOCL_HOSTLIB_TYPES_H
#define XOCL_HOSTLIB_TYPES_H

#include <unordered_map>

#include "xcl2/xcl2.hpp"



// config words
enum MigrateDirection {ToDevice, ToHost};
enum BufferType {ReadOnly, WriteOnly, ReadWrite};
enum ExecMode {SW_EMU, HW_EMU, HW};

// alias for argument map
using ArgumentMap = std::unordered_map<std::string, int>;

// alias for 4k-aligned vector
template<typename T>
using aligned_vector = std::vector<T, aligned_allocator<T> >;


/**
 * @brief The names of the device
 *
 * Each device has two identifiers:
 * - one is the platform name, which is used in building, sw_emu, and hw_emu
 * - the other is the xilinx shell archive (xsa), which is only used when running on hardware
 *
 */
struct BoardIdentifier {
    /**
     * Used in building, sw_emu, and hw_emu
     */
    std::string name;
    /**
     * Only used when running on hardware
     */
    std::string xsa;
};


/**
 * @brief The signature of the kernel function
 */
struct KernelSignature{
    /**
     * Function name
     */
    std::string name;
    /**
     * std::unordered_map from argument name (string) to argument index (int)
     */
    ArgumentMap argmap;
};




#endif //XOCL_HOSTLIB_TYPES_H
