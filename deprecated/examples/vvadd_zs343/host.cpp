#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "vvadd.h"
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

int main(int argc, char** argv) {
    // parse arguments
    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << " <xclbin path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 0;
    }
    std::string xclbin_path = argv[1];
    
    std::vector<xhl::Device> devices = xhl::find_devices(
    xhl::boards::alveo::u280::identifier
    );
    xhl::Device device_1 = devices[0];

    
    devices[0].program_device(xclbin_path);

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

    xhl::Module_vvadd module;
    module.initialize(devices);
    // module.bind_cu(cus_1);
    out = module.run(in1, in2, out);
    bool pass = check_ref(out, out_ref);
    std::cout << (pass ? "[INFO]: Test1 Passed !" : "[ERROR]: Test Failed!\n") 
    << std::endl;
    return 0;
}