#include <iostream>
#include <string>
#include <vector>

#include "device.hpp"
#include "compute_unit.hpp"
#include "xocl-host-lib.hpp"


using namespace xhl::boards;

int main(int argc, char** argv) {
    // usage: host <xclbin_path> <vector size>
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << "<exec mode> <xclbin_path> <vector size>" << std::endl;
        return 1;
    }
    const std::string exec_mode = argv[1];
    const std::string xclbin = argv[2];
    const unsigned size = std::stoi(argv[3]);

    // first see if we are in sw_emu, hw_emu, or hw
    if (exec_mode == "hw") {
        unsetenv("XCL_EMULATION_MODE");
    } else {
        setenv("XCL_EMULATION_MODE", exec_mode.c_str(), 1);
    }

    // find device
    std::vector<xhl::Device> available_u280_devices = xhl::find_devices(alveo::u280::identifier);
    xhl::Device device = available_u280_devices[0];

    // program device (creates all necessary OpenCL objects)
    device.program_device(xclbin);

    // create compute unit using kernel signature
    xhl::ComputeUnit* vvadd_cu = device.find({
        "vvadd", {
            {"a", "float*"},
            {"b", "float*"},
            {"c", "float*"},
            {"size", "unsigned"},
        }
    });

    // prepare data
    srand(0x12345678);
    std::vector<float> a(size), b(size), c(size), c_ref(size);
    for (unsigned i = 0; i < size; ++i) {
        a[i] = (float)rand() / (float)(RAND_MAX/10);
        b[i] = (float)rand() / (float)(RAND_MAX/10);
        c[i] = 0.0;
        c_ref[i] = a[i] + b[i];
    }

    // allocate device memory
    device.create_buffer(
        "a", size * sizeof(float), a.data(),
        xhl::BufferType::ReadOnly, alveo::u280::HBM[0]
    );
    device.create_buffer(
        "b", size * sizeof(float), b.data(),
        xhl::BufferType::ReadOnly, alveo::u280::HBM[1]
    );
    device.create_buffer(
        "c", size * sizeof(float), c.data(),
        xhl::BufferType::WriteOnly, alveo::u280::HBM[2]
    );

    // move data to device
    xhl::sync_data_htod(&device, "a");
    xhl::sync_data_htod(&device, "b");

    // launch the compute unit
    vvadd_cu->launch(
        device.get_buffer("a"),
        device.get_buffer("b"),
        device.get_buffer("c"),
        size
    );
    device.finish_all_tasks();

    // move results back to host
    xhl::sync_data_dtoh(&device, "c");

     // check results
    bool pass = true;
    for (unsigned i = 0; i < size; ++i) {
        if (c[i] != c_ref[i]) {
            std::cerr << "[ERROR]: Result mismatch at index " << i << "!" << std::endl;
            std::cerr << "         Expected: " << c_ref[i] << ", got: " << c[i] << std::endl;
            pass = false;
            break;
        }
    }

    if (pass) {
        std::cout << "Test passed!" << std::endl;
    } else {
        std::cout << "Test failed!" << std::endl;
    }

    // free the compute unit
    delete vvadd_cu;
    return (pass) ? 0 : 1;
}
