#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <map>

#include "xcl2/xcl2.hpp"

#include "device.hpp"

namespace xhl {

/**
 * @brief 4KB aligned vector.
 *
 * This vector align the memory to 4KB boundaries, which leads to
 * fewer PCIe transfers when moving data between host and device.
 *
 * @tparam T data type
 */
template <typename T>
using aligned_vector = std::vector<T, aligned_allocator<T>>;

// config words
/**
 * @brief Data migration direction
 */
enum MigrateDirection {ToDevice, ToHost};
/**
 * @brief Access control for the device
 */
enum BufferType {ReadOnly, WriteOnly, ReadWrite};
/**
 * @brief Execution mode
 */
enum ExecMode {SW_EMU, HW_EMU, HW};

/**
 * @brief check the execution mode (SW, HW_EMU, HW)
 * @return return xhl::enmu type, including SW_EMU, HW_EMU, HW
 */
ExecMode check_execution_mode();

/**
 * @brief
 *
 */
struct BoardIdentifier {
    std::string name;
    std::string xsa;
};

/**
 * @brief
 *
 */
struct KernelSignature {
    // from argument name to argument type, string to string
    std::string name; // kernel name
    std::map<std::string, std::string> argmap; // argument map
};

/**
 * @brief
 *
 * @param identifier
 * @return std::vector<Device>
 */
std::vector<Device> find_devices(const xhl::BoardIdentifier &identifier);

} // namespace xhl


#endif