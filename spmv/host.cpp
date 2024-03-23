#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>
#include <algorithm>
#include <cstdlib>   // For rand() function
#include <ctime>     // For srand() function

#include "xocl-host-library/xocl-host-lib.hpp"
#include "data_loader/data_formatter.h"
#include "data_loader/data_loader.h"

#include "xcl2/xcl2.hpp"
// #define output_size 5
// #define num_cols 5
using namespace xhl;
// template<typename T>
// using aligned_vector = std::vector<T, aligned_allocator<T> >;

#define DATA_SIZE 107614

typedef unsigned IDX_T;

// check buffer
bool check_buffer(
    runtime::Buffer<int> v,
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

//--------------------------------------------------------------------------------------------------
// ground true data
//--------------------------------------------------------------------------------------------------
void compute_ref(
    spmv::io::CSRMatrix<int> &mat,
    std::vector<int> &vector,
    std::vector<int> &ref_result
) {
    ref_result.resize(mat.num_rows);
    std::fill(ref_result.begin(), ref_result.end(), 0);
    for (size_t row_idx = 0; row_idx < mat.num_rows; row_idx++) {
        IDX_T start = mat.adj_indptr[row_idx];
        IDX_T end = mat.adj_indptr[row_idx + 1];
        for (size_t i = start; i < end; i++) {
            IDX_T idx = mat.adj_indices[i];
            ref_result[row_idx] += mat.adj_data[i] * vector[idx];
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Testbench
//--------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    // parse arguments
    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << "<xclbin path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 1;
    }
    std::string xclbin_path = argv[1];

    // set the environment variable
    auto target = check_execution_mode();
    if (target == SW_EMU) {
        setenv("XCL_EMULATION_MODE", "sw_emu", true);
    } else if (target == HW_EMU) {
        setenv("XCL_EMULATION_MODE", "hw_emu", true);
    }

    // generate matrix and tranfer it from float to int
    spmv::io::CSRMatrix<float> mat_f =
        spmv::io::load_csr_matrix_from_float_npz("/home/zs343/zs343/HiSparse/datasets/graph/gplus_108K_13M_csr_float32.npz");

    spmv::io::CSRMatrix<int> mat_i = 
        spmv::io::csr_matrix_convert_from_float<int>(mat_f);

    //--------------------------------------------------------------------
    // generate input vector
    //--------------------------------------------------------------------
    std::vector<int> vector_i(mat_i.num_cols);
    std::generate(vector_i.begin(), vector_i.end(), [&](){return int(rand() % 2);});

    auto devices = xhl::runtime::find_devices(runtime::boards::alveo::u280::identifier);
    runtime::Device device = devices[0];
    device.program_device(xclbin_path);

    std::vector<int> vector_o(mat_i.num_cols);
    runtime::Buffer<int> row_ptr = device.create_buffer<int>("row_ptr",DATA_SIZE,BufferType::ReadOnly,runtime::boards::alveo::u280::HBM[0]);
    runtime::Buffer<int> col_idx = device.create_buffer<int>("col_idx",DATA_SIZE,BufferType::ReadOnly,runtime::boards::alveo::u280::HBM[1]);
    runtime::Buffer<int> values = device.create_buffer<int>("values",DATA_SIZE,BufferType::ReadOnly,runtime::boards::alveo::u280::HBM[2]);
    runtime::Buffer<int> vector_in = device.create_buffer<int>("vector_in",DATA_SIZE,BufferType::ReadOnly,runtime::boards::alveo::u280::HBM[3]);
    runtime::Buffer<int> vector_out = device.create_buffer<int>("vector_out",DATA_SIZE,BufferType::WriteOnly,runtime::boards::alveo::u280::HBM[4]);

    for (size_t i = 0; i < DATA_SIZE; ++i) {
        row_ptr[i] = mat_i.adj_indptr[i];
        col_idx[i] = mat_i.adj_indices[i];
        values[i] = mat_i.adj_data[i];
        vector_in[i] = vector_i[i];
        // std::cout << row_ptr[i] << std::endl;
        // std::cout << mat_i.adj_indptr[i] << std::endl;
    }

    // migrate data
    row_ptr.migrate_data(ToDevice);
    col_idx.migrate_data(ToDevice);
    values.migrate_data(ToDevice);
    vector_in.migrate_data(ToDevice);
    device.command_q.finish();

    struct KernelSignature spmv = {
        "spmv",
        {{"row_ptr",  "int*"},
        {"col_idx",  "int*"},
        {"values",  "int*"},
        {"vector_in", "int*"},
        {"vector_out", "int*"},
        {"num_rows", "int"},
        {"num_cols", "int"}}
    };

    std::vector<int> ref_result;
    compute_ref(mat_i, vector_i, ref_result);

    ComputeUnit cu(spmv);
    cu.bind(&device);

    cu.launch(row_ptr, col_idx, values, vector_in, vector_out, DATA_SIZE, DATA_SIZE);
    device.command_q.finish();
    vector_out.migrate_data(ToHost);

    bool pass = check_buffer(vector_out, ref_result);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    
    std::cout << "INFO : SpMV kernel complete!" << std::endl;

    //--------------------------------------------------------------------
    // compute reference
    //--------------------------------------------------------------------
    
    std::cout << "INFO : Compute reference complete!" << std::endl;

    return 0;
}