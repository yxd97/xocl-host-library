#ifndef XOCL_HOSTLIB_H
#define XOCL_HOSTLIB_H

#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#include "xcl2.hpp"

namespace xhl {
class IBuffer;
class Device;
void nb_sync_data_htod(xhl::Device* d, xhl::IBuffer*);
void nb_sync_data_dtoh(xhl::Device* d, xhl::IBuffer*);
void sync_data_htod(xhl::Device* d, xhl::IBuffer*);
void sync_data_dtoh(xhl::Device* d, xhl::IBuffer*);

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


namespace boards {

namespace alveo {

namespace u280 {

constexpr int CHANNEL_NAME(const int n) { return n | XCL_MEM_TOPOLOGY; }

// instantiate u280 identifier
const static BoardIdentifier identifier = {
    "xilinx_u280_gen3x16_xdma_1_202211_1",
    "xilinx_u280_gen3x16_xdma_base_1"
};

// u280 memory channels
const int HBM[32] = {
    CHANNEL_NAME(0),  CHANNEL_NAME(1),  CHANNEL_NAME(2),  CHANNEL_NAME(3),  CHANNEL_NAME(4),
    CHANNEL_NAME(5),  CHANNEL_NAME(6),  CHANNEL_NAME(7),  CHANNEL_NAME(8),  CHANNEL_NAME(9),
    CHANNEL_NAME(10), CHANNEL_NAME(11), CHANNEL_NAME(12), CHANNEL_NAME(13), CHANNEL_NAME(14),
    CHANNEL_NAME(15), CHANNEL_NAME(16), CHANNEL_NAME(17), CHANNEL_NAME(18), CHANNEL_NAME(19),
    CHANNEL_NAME(20), CHANNEL_NAME(21), CHANNEL_NAME(22), CHANNEL_NAME(23), CHANNEL_NAME(24),
    CHANNEL_NAME(25), CHANNEL_NAME(26), CHANNEL_NAME(27), CHANNEL_NAME(28), CHANNEL_NAME(29),
    CHANNEL_NAME(30), CHANNEL_NAME(31)
};
const int DDR[2] = {CHANNEL_NAME(32), CHANNEL_NAME(33)};

}; // namespace u280

}; // namespace alveo

}; // namespace boards

}; // namespace xhl

#endif //XOCL_HOSTLIB_H