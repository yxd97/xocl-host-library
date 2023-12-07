#ifndef LIBHOST_DEVICES_H
#define LIBHOST_DEVICES_H

#include "xcl2/xcl2.hpp"

namespace alveo {

#define CHANNEL_NAME(n) n | XCL_MEM_TOPOLOGY

namespace u280 {
    // u280 device name
    std::string name = "xilinx_u280_gen3x16_xdma_1_202211_1";
    std::string board_xsa = "xilinx_u280_gen3x16_xdma_base_1";

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
};

} // namespace alveo

#endif //LIBHOST_DEVICES_H
