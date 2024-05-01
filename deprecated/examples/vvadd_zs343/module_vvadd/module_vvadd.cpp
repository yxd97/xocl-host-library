#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "../vvadd.h"
#include "xocl-host-lib.hpp"
#include "device.hpp"
#include "compute_unit.hpp"
#include "module_vvadd.hpp"

namespace xhl{
std::vector<int> Module_vvadd::run(
    std::vector<int> in1, 
    std::vector<int> in2,
    std::vector<int> out
    // Device &device_1
    // ComputeUnit &vvadd_cu
) {
    auto device_1 = this->vvadd_cu->cu_device;
    
    device_1->create_buffer(
        "in1", DATA_SIZE * sizeof(int), in1.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[0]
    );
    device_1->create_buffer(
        "in2", DATA_SIZE * sizeof(int), in2.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
    );
    device_1->create_buffer(
        "out", DATA_SIZE * sizeof(int), out.data(),
        xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[2]
    );

    xhl::sync_data_htod(device_1, "in1");
    xhl::sync_data_htod(device_1, "in2");

    // this->compute_units["vvadd_cu"].launch
    this->vvadd_cu->launch
    (
        device_1->get_buffer("in1"), 
        device_1->get_buffer("in2"), 
        device_1->get_buffer("out"),
        DATA_SIZE
    );
    device_1->finish_all_tasks();

    xhl::sync_data_dtoh(device_1, "out");
    return out;
}

void Module_vvadd::initialize(std::vector<Device>& devices) {
    this->vvadd_cu = devices[0].find(this->vvadd);
    // this->compute_units["vvadd_cu"] = cu_tmp;
}

void Module_vvadd::free_cu(){
    delete this->vvadd_cu;
}

Module_vvadd::~Module_vvadd(){
    this->free_cu();
}

} // namespace xhl