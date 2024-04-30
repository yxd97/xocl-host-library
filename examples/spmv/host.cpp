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
        // else {
        //     std::cout << "[INFO]: Value match!" << std::endl;
        //     std::cout << "         at i = " << i << std::endl;
        //     std::cout << "         Value   : " << v[i] << std::endl;
        
        // }
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
    
    // only for test
    // std::vector<int> adj_data;
    // std::vector<uint32_t> adj_indices;
    // std::vector<uint32_t> adj_indptr;
    // uint32_t num_rows = 3;
    // uint32_t num_cols = 3;
    // for (size_t i = 0; i < 6; ++i) {
    //     adj_data.push_back(i + 1);
    // }
    // adj_indices = {0, 2, 2, 0, 1, 2};
    // adj_indptr = {0, 2, 3, 6};
    // spmv::io::CSRMatrix<int> mat_i = spmv::io::create_csr_matrix(
    //     num_rows, num_cols, 
    //     adj_data, adj_indices, adj_indptr
    // );
    // end test


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
        // [&](){return int(rand() % 2);} 
        [&](){return 1;} // only for test
    );
    // for (size_t i = 0; i < vector_i.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_i[i] << std::endl;
    // }

    // std::vector<xhl::Device> devices;
    // const xhl::BoardIdentifier identifier = xhl::boards::alveo::u280::identifier;
    // std::string target_name =
    //     (xhl::check_execution_mode() == xhl::ExecMode::HW)
    //     ? identifier.xsa : identifier.name;
    // bool found_device = false;
    // auto cl_devices = xcl::get_xil_devices();
    // for (size_t i = 0; i < cl_devices.size(); i++) {
    //     if (cl_devices[i].getInfo<CL_DEVICE_NAME>() == target_name) {
    //         xhl::Device tmp_device;
    //         (tmp_device).bind_device(cl_devices[i]); // use function
    //         devices.push_back(tmp_device);
    //         found_device = true;
    //         std::cout << "device size: " << cl_devices.size() << std:: endl;
    //     }
    // }
    // if (!found_device) {
    //     throw std::runtime_error("[ERROR]: Failed to find Device, exit!\n");
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
        // // only for test
        // {"adj_data_out", "int*"},
        // {"adj_indices_out", "uint32_t*"},
        // {"adj_indptr_out", "uint32_t*"},
        // {"vector_in_out", "int*"},
        // //
        {"num_rows", "int"},
        {"num_cols", "int"}}
    };

    xhl::ComputeUnit spmv_cu(spmv);
    spmv_cu.bind(&device);

    aligned_vector<int> vector_o(mat_i.num_rows); 
    std::fill(vector_o.begin(), vector_o.end(), 0);
    // aligned_vector<int> vector_o(num_cols);

    // only for test
    std::vector<int> adj_data_out(6);
    std::vector<uint32_t> adj_indices_out(6);
    std::vector<uint32_t> adj_indptr_out(4);
    std::vector<int> vector_in_out(mat_i.num_cols);
    
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
        "vector_out", (mat_i.num_rows) * sizeof(int), vector_o.data(), 
        xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[4]
    );
    // // only for test
    // device.create_buffer(
    //     "values_out", (adj_data_out.size()) * sizeof(int), adj_data_out.data(),
    //     xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[5]
    // );
    // device.create_buffer(
    //     "col_idx_out", (adj_indices_out.size()) * sizeof(uint32_t), adj_indices_out.data(),
    //     xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[6]
    // );
    // device.create_buffer(
    //     "row_ptr_out", (adj_indptr_out.size()) * sizeof(uint32_t), adj_indptr_out.data(),
    //     xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[7]
    // );
    // device.create_buffer(
    //     "vector_in_out", (mat_i.num_cols) * sizeof(int), vector_in_out.data(),
    //     xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[8]
    // );
    // // output mat_i.adj_data
    // for (size_t i = 0; i < mat_i.adj_data.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_data[i] << std::endl;
    // }
    // // output mat_i.adj_indices
    // for (size_t i = 0; i < mat_i.adj_indices.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_indices[i] << std::endl;
    // }
    // // output mat_i.adj_indptr
    // for (size_t i = 0; i < mat_i.adj_indptr.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_indptr[i] << std::endl;
    // }
    // // output vector_i
    // for (size_t i = 0; i < vector_i.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_i[i] << std::endl;
    // }

    // // output ext_ptrs
    // for (auto it = device._ext_ptrs.begin(); it != device._ext_ptrs.end(); ++it) {
    //     // std::cout << "ext_ptrs: " << it->first << "\t" << ((int*)(it->second.obj))[2] << std::endl;
    //     std::cout << "ext_ptrs: " << it->first << "\t" << ((int)(it->second.flags)) << std::endl;
    // }
    // std::cout << xhl::boards::alveo::u280::HBM[0] 
    //           << "\t" << xhl::boards::alveo::u280::HBM[1] 
    //           << "\t" << xhl::boards::alveo::u280::HBM[2] 
    //           << "\t" << xhl::boards::alveo::u280::HBM[3] 
    //           << "\t" << xhl::boards::alveo::u280::HBM[4] 
    //           << "\t" << std::endl;

    xhl::sync_data_htod(&device, "values");
    xhl::sync_data_htod(&device, "col_idx");
    xhl::sync_data_htod(&device, "row_ptr");
    xhl::sync_data_htod(&device, "vector_in");

    // spmv_cu.launch(
    //     device.get_buffer("values"), 
    //     device.get_buffer("col_idx"), 
    //     device.get_buffer("row_ptr"), 
    //     device.get_buffer("vector_in"), 
    //     device.get_buffer("vector_out"),
    //     mat_i.num_rows,
    //     mat_i.num_cols
    // );
    spmv_cu.launch(
        device.get_buffer("values"),
        device.get_buffer("col_idx"),
        device.get_buffer("row_ptr"),
        device.get_buffer("vector_in"),
        device.get_buffer("vector_out"),
        // // only for test
        // device._buffers["values_out"],
        // device._buffers["col_idx_out"],
        // device._buffers["row_ptr_out"],
        // device._buffers["vector_in_out"],
        // //
        mat_i.num_rows,
        mat_i.num_cols
    );
    device.finish_all_tasks();
    xhl::sync_data_dtoh(&device, "vector_out");
    // // only for test
    // xhl::sync_data_dtoh(&device, "values_out");
    // xhl::sync_data_dtoh(&device, "col_idx_out");
    // xhl::sync_data_dtoh(&device, "row_ptr_out");
    // xhl::sync_data_dtoh(&device, "vector_in_out");
    // 
    // output adj_data_out
    // std::cout << "adj_data_out:" << std::endl;
    // for (size_t i = 0; i < adj_data_out.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << adj_data_out[i] << std::endl;
    // }
    // // output adj_indices_out
    // std::cout << "adj_indices_out:" << std::endl;
    // for (size_t i = 0; i < adj_indices_out.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << adj_indices_out[i] << std::endl;
    // }
    // // output adj_indptr_out
    // std::cout << "adj_indptr_out:" << std::endl;
    // for (size_t i = 0; i < adj_indptr_out.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << adj_indptr_out[i] << std::endl;
    // }
    // // output vector_in_out
    // std::cout << "vector_in_out:" << std::endl;
    // for (size_t i = 0; i < vector_in_out.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_in_out[i] << std::endl;
    // }

    aligned_vector<int> ref_result;
    compute_ref(mat_i, vector_i, ref_result);
    // // output vector_i
    // for (size_t i = 0; i < vector_i.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_i[i] << std::endl;
    // }
    // // output ref_result
    // for (size_t i = 0; i < ref_result.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << ref_result[i] << std::endl;
    // }
    // // output vector_o
    // for (size_t i = 0; i < vector_o.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_o[i] << std::endl;
    // }
    // // output mat_i.adj_data
    // for (size_t i = 0; i < mat_i.adj_data.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_data[i] << std::endl;
    // }
    // // output mat_i.adj_indices
    // for (size_t i = 0; i < mat_i.adj_indices.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_indices[i] << std::endl;
    // }
    // // output mat_i.adj_indptr
    // for (size_t i = 0; i < mat_i.adj_indptr.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << mat_i.adj_indptr[i] << std::endl;
    // }
    // // output vector_i
    // for (size_t i = 0; i < vector_i.size(); ++i) {
    //     std::cout << "i: " << i << "\t" << vector_i[i] << std::endl;
    // }

    bool pass = check_buffer(vector_o, ref_result);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    
    std::cout << "INFO : SpMV kernel complete!" << std::endl;

    //--------------------------------------------------------------------
    // compute reference
    //--------------------------------------------------------------------
    
    std::cout << "INFO : Compute reference complete!" << std::endl;

    return 0;
}