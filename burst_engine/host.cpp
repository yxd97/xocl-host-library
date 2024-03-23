#include <algorithm>
#include <cmath>
#include <chrono>
#include <numeric>
#include <vector>
#include <iostream>

#include "xocl-host-lib.hpp"
#include "device.hpp"
#include "buffer.hpp"
#include "compute_unit.hpp"

const uint64_t DATA_SIZE_IN_BYTES = 1024 * 1024 * 1024; // must be multiples of 64
const uint64_t NUM_RUNS = 10; // number of runs for each test

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <xclbin>" << std::endl;
        return 1;
    }

    std::vector<xhl::Device> devices = xhl::find_devices(
        xhl::boards::alveo::u280::identifier
    );
    xhl::Device device = devices[0];

    device.program_device(argv[1]);

    xhl::KernelSignature burst_engine = {
        "burst_engine", {
            {"location" ,"ap_int<512>*"},
            {"size_in_bytes", "uint64_t"},
            {"read_or_write", "int"}
        }
    };

    xhl::ComputeUnit burst_engine_cu(burst_engine);
    burst_engine_cu.bind(&device);

    xhl::Buffer<uint8_t> buf = device.create_buffer<uint8_t>(
        "location", DATA_SIZE_IN_BYTES,
        xhl::BufferType::ReadWrite, xhl::boards::alveo::u280::DDR[0]
    );
    std::generate(buf.begin(), buf.end(), [n = 0]() mutable { return n++; });
    buf.migrate_data(xhl::MigrateDirection::ToDevice);

    auto start = std::chrono::high_resolution_clock::now();
    burst_engine_cu.launch(buf, DATA_SIZE_IN_BYTES, 1);
    device.finish_all_tasks();
    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Duration: " << duration_ms << " ms" << std::endl;

    return 0;
}