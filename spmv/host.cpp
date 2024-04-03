#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>
#include <algorithm>
#include <cstdlib>   // For rand() function
#include <ctime>     // For srand() function

#include "xocl-host-lib.hpp"
#include "device.hpp"
#include "compute_unit.hpp"
#include "data_formatter.h"
#include "data_loader.h"

#include "xcl2.hpp"
// #define output_size 5
// #define num_cols 5
template<typename T>
using aligned_vector = std::vector<T, aligned_allocator<T> >;

#define DATA_SIZE 107614

typedef unsigned IDX_T;

// check buffer
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

//-----------------------------------------------------------------------------
// ground true data
//-----------------------------------------------------------------------------
void compute_ref(
    spmv::io::CSRMatrix<int> &mat,
    aligned_vector<int> &vector,
    aligned_vector<int> &ref_result
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

//----------------------------------------------------------------------------
// Testbench
//----------------------------------------------------------------------------
int main(int argc, char** argv) {
    // parse arguments
    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << "<xclbin path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 1;
    }

    // generate matrix and tranfer it from float to int
    spmv::io::CSRMatrix<float> mat_f =
        spmv::io::load_csr_matrix_from_float_npz(
        "/home/zs343/zs343/HiSparse/datasets/graph/gplus_108K_13M_csr_float32.npz"
        );

    spmv::io::CSRMatrix<int> mat_i = 
        spmv::io::csr_matrix_convert_from_float<int>(mat_f);
    // for (size_t i = 0; i < mat_i.adj_data.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_data[i] << std::endl;
    // }

    //--------------------------------------------------------------------
    // generate input vector
    //--------------------------------------------------------------------
    aligned_vector<int> vector_i(mat_i.num_cols);
    std::generate(
        vector_i.begin(), 
        vector_i.end(), 
        [&](){return int(rand() % 2);}
    );
    // for (size_t i = 0; i < vector_i.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_i[i] << std::endl;
    // }

    std::vector<xhl::Device> devices = xhl::find_devices(
        xhl::boards::alveo::u280::identifier
    );
    xhl::Device device = devices[0];

    device.program_device(argv[1]);

    xhl::KernelSignature spmv = {
        "spmv",
        {{"values",  "int*"},
        {"col_idx",  "int*"},
        {"row_ptr",  "int*"},
        {"vector_in", "int*"},
        {"vector_out", "int*"},
        {"num_rows", "int"},
        {"num_cols", "int"}}
    };

    xhl::ComputeUnit spmv_cu(spmv);
    spmv_cu.bind(&device);

    aligned_vector<int> vector_o(mat_i.num_cols);
    
    device.create_buffer(
        "values", (mat_i.adj_data.size()) * sizeof(int), mat_i.adj_data.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[0]
    );
    device.create_buffer(
        "col_idx", (mat_i.adj_indices.size()) * sizeof(int), mat_i.adj_indices.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
    );
    device.create_buffer(
        "row_ptr", (mat_i.adj_indptr.size()) * sizeof(int), mat_i.adj_indptr.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[2]
    );
    device.create_buffer(
        "vector_in", (mat_i.num_cols) * sizeof(int), vector_i.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[3]
    );
    device.create_buffer(
        "vector_out", (mat_i.num_cols) * sizeof(int), vector_o.data(),
        xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[4]
    );

    xhl::sync_data_htod(&device, "values");
    xhl::sync_data_htod(&device, "col_idx");
    xhl::sync_data_htod(&device, "row_ptr");
    xhl::sync_data_htod(&device, "vector_in");

    spmv_cu.launch(
        device.get_buffer("values"), 
        device.get_buffer("col_idx"), 
        device.get_buffer("row_ptr"), 
        device.get_buffer("vector_in"), 
        device.get_buffer("vector_out"),
        mat_i.num_rows,
        mat_i.num_cols
    );
    device.finish_all_tasks();
    xhl::sync_data_dtoh(&device, "vector_out");

    // for (size_t i = 0; i < vector_i.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_i[i] << std::endl;
    // }

    aligned_vector<int> ref_result;
    compute_ref(mat_i, vector_i, ref_result);

    bool pass = check_buffer(vector_o, ref_result);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    
    std::cout << "INFO : SpMV kernel complete!" << std::endl;

    //--------------------------------------------------------------------
    // compute reference
    //--------------------------------------------------------------------
    
    std::cout << "INFO : Compute reference complete!" << std::endl;

    return 0;
}