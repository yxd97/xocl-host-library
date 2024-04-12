#include "device.hpp"
#include "compute_unit.hpp"

#include "util.h"
#include "vvadd.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <thread>

#include "xcl2.hpp"

using namespace xhl;

//--------------------------------------------------------------------------------------------------
// Utilities
//--------------------------------------------------------------------------------------------------
const struct KernelSignature vvadd = {
    "vvadd",
    {{"in1",  "int*"},
    {"in2",  "int*"},
    {"out",  "int*"},
    {"size", "int"}}
};
bool check_buffer(
    aligned_vector<int> v,
    aligned_vector<int> ref,
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

bool test_parallel_vvadd(std::vector<Device>& devices,
                         std::vector<aligned_vector<int>>& in1s,
                         std::vector<aligned_vector<int>>& in2s,
                         std::vector<aligned_vector<int>>& outs) {
    // create buffers
    std::vector<aligned_vector<int>> refs;
    for (size_t ind = 0; ind < devices.size(); ind++) {
        refs.push_back(aligned_vector<int>(DATA_SIZE));

        // generate inputs/outputs
        std::generate(
            in1s[ind].begin(), in1s[ind].end(),
            [](){return rand() % 10;}
        );
        std::generate(
            in2s[ind].begin(), in2s[ind].end(),
            [](){return rand() % 10;}
        );
        for (size_t i = 0; i < DATA_SIZE; i++) {
            refs[ind][i] = in1s[ind][i] + in2s[ind][i];
        }
    }
    
    // migrate data
    for (Device& device : devices) {
        nb_sync_data_htod(&device, "in1");
        nb_sync_data_htod(&device, "in2");
    }
    for (Device& device : devices)
        device.command_q.finish();
    
    std::vector<ComputeUnit> cus;
    for (size_t i = 0; i < devices.size(); i++) {
        cus.push_back(ComputeUnit(vvadd));
        cus[i].bind(&devices[i]);
    }

    for (ComputeUnit cu : cus)
        cu.launch(cu.cu_device->get_buffer("in1"),
                  cu.cu_device->get_buffer("in2"),
                  cu.cu_device->get_buffer("out"),
                  DATA_SIZE);
    for (Device& device : devices)
        device.command_q.finish();
    for (Device& device : devices) {
        nb_sync_data_dtoh(&device, "out");
    }
    for (Device& device : devices)
        device.command_q.finish();

    bool pass = true;
    for (size_t i = 0; i < devices.size(); i++)
        pass = pass && check_buffer(outs[i], refs[i]);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    return pass;
}

bool test_2device_3vector_1thread_vvadd_reduction(std::vector<Device>& devices,
                                                  Link& link_01, Link& link_10,
                                                  std::vector<aligned_vector<int>>& in1s,
                                                  std::vector<aligned_vector<int>>& in2s,
                                                  std::vector<aligned_vector<int>>& outs) {
    for (size_t ind = 0; ind < devices.size(); ind++) {
        // generate inputs/outputs
        std::generate(
            in1s[ind].begin(), in1s[ind].end(),
            [](){return rand() % 10;}
        );
        std::generate(
            in2s[ind].begin(), in2s[ind].end(),
            [](){return rand() % 10;}
        );
    }
    aligned_vector<int> ref(DATA_SIZE);
    for (size_t i = 0; i < DATA_SIZE; i++) {
        ref[i] = in1s[0][i] + in2s[0][i] + in2s[1][i];
    }

    nb_sync_data_htod(&devices[0], "in1");
    nb_sync_data_htod(&devices[0], "in2");
    nb_sync_data_htod(&devices[1], "in2");
    for (Device& device : devices)
        device.command_q.finish();

    std::vector<ComputeUnit> cus;
    for (size_t i = 0; i < devices.size(); i++) {
        cus.push_back(ComputeUnit(vvadd));
        cus[i].bind(&devices[i]);
    }
    
    cus[0].launch(cus[0].cu_device->get_buffer("in1"),
                cus[0].cu_device->get_buffer("in2"),
                cus[0].cu_device->get_buffer("out"),
                DATA_SIZE);
    devices[0].command_q.finish();
    link_01.transfer("out", "in1");
    link_01.finish();

    link_10.wait();
    cus[1].launch(cus[1].cu_device->get_buffer("in1"),
                cus[1].cu_device->get_buffer("in2"),
                cus[1].cu_device->get_buffer("out"),
                DATA_SIZE);
    devices[1].command_q.finish();

    sync_data_dtoh(&devices[1], "out");

    bool pass = check_buffer(outs[1], ref);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    return pass;
}

bool test_2device_3vector_2thread_vvadd_reduction(std::vector<Device>& devices,
                                                  Link& link_01, Link& link_10,
                                                  std::vector<aligned_vector<int>>& in1s,
                                                  std::vector<aligned_vector<int>>& in2s,
                                                  std::vector<aligned_vector<int>>& outs) {
    for (size_t ind = 0; ind < devices.size(); ind++) {
        // generate inputs/outputs
        std::generate(
            in1s[ind].begin(), in1s[ind].end(),
            [](){return rand() % 10;}
        );
        std::generate(
            in2s[ind].begin(), in2s[ind].end(),
            [](){return rand() % 10;}
        );
    }
    aligned_vector<int> ref(DATA_SIZE);
    for (size_t i = 0; i < DATA_SIZE; i++) {
        ref[i] = in1s[0][i] + in2s[0][i] + in2s[1][i];
    }

    nb_sync_data_htod(&devices[0], "in1");
    nb_sync_data_htod(&devices[0], "in2");
    nb_sync_data_htod(&devices[1], "in2");
    for (Device& device : devices)
        device.command_q.finish();

    std::vector<ComputeUnit> cus;
    for (size_t i = 0; i < devices.size(); i++) {
        cus.push_back(ComputeUnit(vvadd));
        cus[i].bind(&devices[i]);
    }
    
    std::thread thread_d0 ([&] {
        cus[0].launch(cus[0].cu_device->get_buffer("in1"),
                    cus[0].cu_device->get_buffer("in2"),
                    cus[0].cu_device->get_buffer("out"),
                    DATA_SIZE);
        devices[0].command_q.finish();
        link_01.transfer("out", "in1");
        link_01.finish();
    });
    std::thread thread_d1 ([&] {
        link_10.wait();
        cus[1].launch(cus[1].cu_device->get_buffer("in1"),
                    cus[1].cu_device->get_buffer("in2"),
                    cus[1].cu_device->get_buffer("out"),
                    DATA_SIZE);
        devices[1].command_q.finish();
    });
    
    thread_d0.join();
    thread_d1.join();

    sync_data_dtoh(&devices[1], "out");

    bool pass = check_buffer(outs[1], ref);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    return pass;
}

//--------------------------------------------------------------------------------------------------
// Testbench
//--------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    // parse arguments
    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << " <xclbin path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 0;
    }
    std::string xclbin_path = argv[1];

    // set the environment variable
    auto target = check_execution_mode();
    if (target == SW_EMU) {
        setenv("XCL_EMULATION_MODE", "sw_emu", true);
    } else if (target == HW_EMU) {
        setenv("XCL_EMULATION_MODE", "hw_emu", true);
    }

    std::vector<Device> devices = find_devices(boards::alveo::u280::identifier);
    for (Device& device : devices) {
        device.program_device(xclbin_path);
    }
    
    std::vector<aligned_vector<int>> in1s;
    std::vector<aligned_vector<int>> in2s;
    std::vector<aligned_vector<int>> outs;
    for (size_t ind = 0; ind < devices.size(); ind++) {
        in1s.push_back(aligned_vector<int>(DATA_SIZE));
        in2s.push_back(aligned_vector<int>(DATA_SIZE));
        outs.push_back(aligned_vector<int>(DATA_SIZE));

        devices[ind].create_buffer("in1", DATA_SIZE*sizeof(int), in1s[ind].data(), BufferType::ReadOnly , boards::alveo::u280::HBM[0]);
        devices[ind].create_buffer("in2", DATA_SIZE*sizeof(int), in2s[ind].data(), BufferType::ReadOnly , boards::alveo::u280::HBM[1]);
        devices[ind].create_buffer("out", DATA_SIZE*sizeof(int), outs[ind].data(), BufferType::WriteOnly, boards::alveo::u280::HBM[2]);
    }
    
    bool all_pass = true;

    std::cout << "Parallel VVAdd Test" << std::endl;
    all_pass = all_pass && test_parallel_vvadd(devices, in1s, in2s, outs);
    
    std::unique_ptr<Link> link_01, link_10;

    link_01 = std::make_unique<HostMemoryLink>(&devices[0], &devices[1]);
    link_10 = std::make_unique<HostMemoryLink>(&devices[1], &devices[0]);

    std::cout << "2 Device 3 Vector 1 Thread VVAdd Reduction via HostMemoryLink" << std::endl;
    all_pass = all_pass
       && test_2device_3vector_1thread_vvadd_reduction(devices, *link_01, *link_10, in1s, in2s, outs);


    std::thread t0([&]{
        link_10 = std::make_unique<HostTCPLink>(&devices[1], "localhost", 7217, true);
    });
    std::thread t1([&]{
        link_01 = std::make_unique<HostTCPLink>(&devices[0], "localhost", 7217, false);
    });
    t0.join();
    t1.join();

    std::cout << "2 Device 3 Vector 1 Thread VVAdd Reduction via HostTCPLink" << std::endl;
    all_pass = all_pass
       && test_2device_3vector_1thread_vvadd_reduction(devices, *link_01, *link_10, in1s, in2s, outs);

        
    std::cout << "2 Device 3 Vector 2 Thread VVAdd Reduction via HostTCPLink" << std::endl;
    all_pass = all_pass
        && test_2device_3vector_2thread_vvadd_reduction(devices, *link_01, *link_10, in1s, in2s, outs);

    return all_pass ? 0 : 1;
}
