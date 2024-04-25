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

int main(int argc, char** argv) {
    // parse arguments
    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << " <xclbin path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 0;
    }
    std::string xclbin_path = argv[1];

    xhl::Module_vvadd module;
    bool pass = module.host(xclbin_path);
    std::cout << (pass ? "[INFO]: Test1 Passed !" : "[ERROR]: Test Failed!\n") 
    << std::endl;
    return 0;
}