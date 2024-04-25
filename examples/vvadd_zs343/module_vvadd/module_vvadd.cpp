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

//--------------------------------------------------------------------------------------------------
// Utilities
//--------------------------------------------------------------------------------------------------
bool check_ref(
    std::vector<int> v,
    std::vector<int> ref,
    bool stop_on_mismatch = true
) {
    // check length
    if (v.size() != ref.size()) {
        std::cout << "[ERROR]: Size mismatch!" << std::endl;
        std::cout << "         Expected: " << ref.size() << std::endl;
        std::cout << "         Actual  : " << v.size() << std::endl;
        return false;
    }

    // check items
    bool match = true;
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] != ref[i]) {
            std::cout << "[ERROR]: Value mismatch!" << std::endl;
            std::cout << "         at i = " << i << std::endl;
            std::cout << "         Expected: " << ref[i] << std::endl;
            std::cout << "         Actual  : " << v[i] << std::endl;
            if (stop_on_mismatch) return false;
            match = false;
        }
    }
    return match;
}

namespace xhl{
bool Module_vvadd::host(std::string xcl_bin_path) {
    std::vector<xhl::Device> devices = xhl::find_devices(
    xhl::boards::alveo::u280::identifier
    );
    xhl::Device device_1 = devices[0];

    std::vector<KernelSignature> signatures;
    xhl::KernelSignature vvadd = {
        "vvadd", {
            {"in1", "int*"},
            {"in2", "int*"},
            {"out", "int*"},
            {"DATA_SIZE", "int"}
        }
    };
    signatures.push_back(vvadd);
    std::vector<xhl::ComputeUnit> cus;
    cus = device_1.program_device(xcl_bin_path, signatures);
    auto vvadd_cu = cus[0];
    // xhl::ComputeUnit vvadd_cu(vvadd);
    // vvadd_cu.bind(&device_1);
    
    // combine cus_1 into cu on Module
    std::unordered_map<std::string, xhl::ComputeUnit> cus_1 = {
        {"vvadd", cus[0]}
    };
    this->bind_cu(cus_1);

    // generate inputs/outputs
    std::vector<int> in1(DATA_SIZE);
    std::vector<int> in2(DATA_SIZE);
    std::vector<int> out_ref(DATA_SIZE);
    std::vector<int> out(DATA_SIZE);

    std::generate(
        in1.begin(), in1.end(),
        [](){return rand() % 10;}
    );
    std::generate(
        in2.begin(), in2.end(),
        [](){return rand() % 10;}
    );
    for (size_t i = 0; i < out_ref.size(); i++) {
        out_ref[i] = in1[i] + in2[i];
    }

    device_1.create_buffer(
        "in1", DATA_SIZE * sizeof(int), in1.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[0]
    );
    device_1.create_buffer(
        "in2", DATA_SIZE * sizeof(int), in2.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
    );
    device_1.create_buffer(
        "out", DATA_SIZE * sizeof(int), out.data(),
        xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[2]
    );

    xhl::sync_data_htod(&device_1, "in1");
    xhl::sync_data_htod(&device_1, "in2");

    vvadd_cu.launch(
        device_1.get_buffer("in1"), 
        device_1.get_buffer("in2"), 
        device_1.get_buffer("out"),
        DATA_SIZE
    );
    device_1.finish_all_tasks();

    xhl::sync_data_dtoh(&device_1, "out");
    bool pass = check_ref(out, out_ref);
    return pass;
}
}