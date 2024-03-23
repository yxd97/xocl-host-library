#include <algorithm>
#include <cmath>
#include <chrono>
#include <numeric>
#include <vector>
#include <iostream>


#include "xcl2.hpp"
const uint64_t DATA_SIZE_IN_BYTES = 1024 * 1024 * 1024; // must be multiples of 64
const uint64_t NUM_RUNS = 10; // number of runs for each test

struct environment {
    cl::Context* ctx;
    cl::CommandQueue* cq;
    cl::Kernel* knl;
    cl::Buffer* buf_ddr0;
    cl::Buffer* buf_ddr1;
};

double run_burst_engine(
    environment &env,
    std::string read_or_write,
    std::string channel,
    uint64_t size_in_bytes
) {
    // Set the kernel arguments
    int rdwr;
    if (read_or_write == "read") {
        rdwr = 1;
    } else if (read_or_write == "write") {
        rdwr = 0;
    } else {
        std::cerr << "Invalid read_or_write argument!" << std::endl;
        exit(-1);
    }
    cl_int err = 0;
    if (channel == "ddr0") {
        err = env.knl->setArg(0, *env.buf_ddr0);
    } else if (channel == "ddr1") {
        err = env.knl->setArg(0, *env.buf_ddr1);
    } else {
        std::cerr << "Invalid channel argument!" << std::endl;
        exit(-1);
    }
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to set kernel argument 0 (location)!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }
    err = 0;
    err = env.knl->setArg(1, size_in_bytes);
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to set kernel argument 1 (size_in_bytes)!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }
    err = 0;
    err = env.knl->setArg(2, rdwr);
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to set kernel argument 2 (read_or_write)!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }
    // launch the kernel
    auto start = std::chrono::high_resolution_clock::now();
    err = env.cq->enqueueTask(*env.knl, NULL, NULL);
    env.cq->finish();
    auto end = std::chrono::high_resolution_clock::now();
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to launch kernel!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return duration_ms;
}

std::string format_size(uint64_t size) {
    std::string unit = "B";
    if (size >= 1024) {
        size /= 1024;
        unit = "KB";
    }
    if (size >= 1024) {
        size /= 1024;
        unit = "MB";
    }
    if (size >= 1024) {
        size /= 1024;
        unit = "GB";
    }
    return std::to_string(size) + " " + unit;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <xclbin>" << std::endl;
        return 1;
    }

    // set the environment variable
    // auto target = check_execution_mode();
    // if (target == SW_EMU) {
    //     setenv("XCL_EMULATION_MODE", "sw_emu", true);
    // } else if (target == HW_EMU) {
    //     setenv("XCL_EMULATION_MODE", "hw_emu", true);
    // }
    // auto devices = xhl::runtime::find_devices(runtime::boards::alveo::u280::identifier);
    // runtime::Device device = devices[0];

    // xcl::get_xil_devices() returns a list of Xilinx devices
    auto devices = xcl::get_xil_devices();
    // find the device we want to program, here U280 is used as an example
    // board name is the string passed to Vitis when building the bitstream, used for loading emulation model
    const std::string u280_board_name = "xilinx_u280_gen3x16_xdma_1_202211_1";
    // Xilinx Shell Archive (XSA) is the identifier of the real board, used for running actual bitstream
    const std::string u280_board_xsa = "xilinx_u280_gen3x16_xdma_base_1";
    // xcl::is_emulation() returns true if we are running in software or hardware emulation
    std::string target_name = xcl::is_emulation() ? u280_board_name : u280_board_xsa;
    bool found_device = false;
    int dev_id = 0;
    for (size_t i = 0; i < devices.size(); i++) {
        if (devices[i].getInfo<CL_DEVICE_NAME>() == target_name) {
            found_device = true;
            dev_id = i;
            break;
        }
    }
    if (!found_device) {
        std::cerr << "[ERROR]: Failed to find " << target_name << ", exit!" << std::endl;
        exit(-1);
    }

    // device.program_device(argv[1]);
    // First, we need to create an OpenCL context for the device we want to program.
    cl::Context ctx = cl::Context(devices[dev_id], NULL, NULL, NULL);
    // Then we read the bitstream file into a array of bytes.
    const std::string bitstream_file_name = argv[1];
    std::vector<unsigned char> file_buf = xcl::read_binary_file(bitstream_file_name);
    // Create a program object from the raw bytes.
    cl::Program::Binaries binary{{file_buf.data(), file_buf.size()}};
    // Program the device with the program object.
    cl_int err = 0;
    cl::Program program(ctx, {devices[dev_id]}, binary, NULL, &err);
    // check if the programming is successful
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to program device with xclbin file!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }

    // Create a command queue for the device.
    // All data movements and kernel executions are issued through the command queue.
    err = 0;
    cl::CommandQueue cq(
        ctx, devices[dev_id],
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE,
        &err
    );

    // std::vector<uint8_t> data_ddr0(DATA_SIZE_IN_BYTES);
    // std::vector<uint8_t> data_ddr1(DATA_SIZE_IN_BYTES);

    // ComputeUnit cu(burst_engine);
    // cu.bind(&device);

    // Create a kernel object for the kernel we want to run.
    const std::string kernel_name = "burst_engine";
    err = 0;
    cl::Kernel knl(program, kernel_name.c_str(), &err);
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to create kernel " << kernel_name << "!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }

    // Allocame memory for the kernel to access on both DDR channels
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_EXT_PTR_XILINX;

    // for DDR channel 0
    std::vector<uint8_t> data_ddr0(DATA_SIZE_IN_BYTES);
    err = 0;
    cl_mem_ext_ptr_t ext_ddr0;
    ext_ddr0.flags = 32 | XCL_MEM_TOPOLOGY;
    ext_ddr0.obj = data_ddr0.data(); // associate it with the data pointer
    ext_ddr0.param = 0;
    cl::Buffer buf_ddr0(ctx, flags, DATA_SIZE_IN_BYTES, &ext_ddr0, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to allocate buffer for DDR channel 0!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }

    // for DDR channel 1
    std::vector<uint8_t> data_ddr1(DATA_SIZE_IN_BYTES);
    err = 0;
    cl_mem_ext_ptr_t ext_ddr1;
    ext_ddr1.flags = 33 | XCL_MEM_TOPOLOGY;
    ext_ddr1.obj = data_ddr1.data(); // associate it with the data pointer
    ext_ddr1.param = 0;
    cl::Buffer buf_ddr1(ctx, flags, DATA_SIZE_IN_BYTES, &ext_ddr1, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "[ERROR]: Failed to allocate buffer for DDR channel 1!" << std::endl;
        std::cerr << "         Error code: " << err << std::endl;
        exit(-1);
    }

    cq.enqueueMigrateMemObjects({buf_ddr0, buf_ddr1}, 0 /* 0 means from host*/);

    // test bandwidth
    environment env = {
        &ctx,
        &cq,
        &knl,
        &buf_ddr0,
        &buf_ddr1
    };

    std::vector<std::string> read_or_write = {"read", "write"};
    std::vector<std::string> channel = {"ddr0", "ddr1"};
    for (auto rw : read_or_write) {
        for (auto ch : channel) {
            for (uint64_t size = 64; size <= DATA_SIZE_IN_BYTES; size*=2) {
                std::cout << "[INFO]: Testing " << rw << " bandwidth on " << ch << " (" << format_size(size) << ")" << std::endl;
                std::vector<double> duration_ms(NUM_RUNS);
                std::vector<double> bw_mbps(NUM_RUNS);
                for (uint64_t run = 0; run < NUM_RUNS; run++) {
                    duration_ms[run] = run_burst_engine(env, rw, ch, size);
                    bw_mbps[run] = size / duration_ms[run] * 1000 / 1024 / 1024;
                }
                double avg_bw = std::accumulate(bw_mbps.begin(), bw_mbps.end(), 0.0) / double(NUM_RUNS);
                std::cout << "        Average bandwidth: " << avg_bw << " MB/s" << std::endl;
                double std_dev = 0;
                for (auto bw : bw_mbps) {
                    std_dev += (bw - avg_bw) * (bw - avg_bw);
                }
                std_dev = sqrt(std_dev / double(NUM_RUNS));
                std::cout << "        Standard deviation: " << std_dev << " MB/s" << std::endl;
            }
        }
    }
    for (auto rw : read_or_write) {
        for (auto ch : channel) {
            std::cout << "[INFO]: Testing " << rw << " latency on " << ch << std::endl;
            std::vector<double> duration_ms(NUM_RUNS);
            for (uint64_t run = 0; run < NUM_RUNS; run++) {
                duration_ms[run] = run_burst_engine(env, rw, ch, 64);
            }
            double avg_duration = std::accumulate(duration_ms.begin(), duration_ms.end(), 0.0) / double(NUM_RUNS);
            std::cout << "        Average latency: " << avg_duration << " ms" << std::endl;
            double std_dev = 0;
            for (auto dur : duration_ms) {
                std_dev += (dur - avg_duration) * (dur - avg_duration);
            }
            std_dev = sqrt(std_dev / double(NUM_RUNS));
            std::cout << "        Standard deviation: " << std_dev << " ms" << std::endl;
        }
    }
    return 0;
}