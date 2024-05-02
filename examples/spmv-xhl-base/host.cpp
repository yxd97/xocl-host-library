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
#include "data_loader.h"

#include "timer.h"

#include "xcl2.hpp"

typedef unsigned IDX_T;
// check buffer
bool check_buffer(
    xhl::aligned_vector<int> v,
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

//-----------------------------------------------------------------------------
// ground true data
//-----------------------------------------------------------------------------
void compute_ref(
    spmv::io::CSRMatrix<int> &mat,
    std::vector<int> &vector,
    std::vector<int> &ref_result
) {
    ref_result.resize(mat.num_rows);
    std::fill(ref_result.begin(), ref_result.end(), 0);
    for (size_t row_idx = 0; row_idx < mat.num_rows; row_idx++) {
        uint32_t start = mat.adj_indptr[row_idx];
        uint32_t end = mat.adj_indptr[row_idx + 1];
        for (size_t i = start; i < end; i++) {
            uint32_t idx = mat.adj_indices[i];
            ref_result[row_idx] += mat.adj_data[i] * vector[idx];
        }
    }
}
void compute_ref(
    spmv::io::CSRMatrix<int> &mat,
    xhl::aligned_vector<int> &vector,
    std::vector<int> &ref_result,
    size_t iterations
) {
    ref_result.resize(mat.num_rows);
    std::vector<int> vec0(vector.begin(), vector.end()), vec1(vector.size());
    for (size_t i = 0; i < iterations; i++)
        compute_ref(mat, i%2==0 ? vec0 : vec1, i%2==0 ? vec1 : vec0);
    std::copy((iterations%2==0 ? vec0 : vec1).begin(), (iterations%2==0 ? vec0 : vec1).end(), ref_result.begin());
}

//----------------------------------------------------------------------------
// Testbench
//----------------------------------------------------------------------------
int main(int argc, char** argv) {
    const int N = 3;
    // parse arguments
    if(argc < 3) {
        std::cout << "Usage : " << argv[0] << " <xclbin path> <dataset path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 1;
    }

    //--------------------------------------------------------------------
    // loading matrix data
    //--------------------------------------------------------------------
    std::cout << "INFO : Loading Dataset" << std::endl;
    spmv::io::CSRMatrix<float> mat_f =
        spmv::io::load_csr_matrix_from_float_npz(argv[2]);
    spmv::io::CSRMatrix<int> mat_i = 
        spmv::io::csr_matrix_convert_from_float<int>(
            mat_f
        );
    
    //--------------------------------------------------------------------
    // generate input vector
    //--------------------------------------------------------------------
    xhl::aligned_vector<int> vector_in(mat_i.num_cols); 
    std::generate(
        vector_in.begin(), 
        vector_in.end(), 
        [&](){return int(rand() % 2);} 
    );

    //--------------------------------------------------------------------
    // compute reference
    //--------------------------------------------------------------------
    std::vector<int> ref_result;
    compute_ref(mat_i, vector_in, ref_result, N);
    std::cout << "INFO : Compute reference complete!" << std::endl;

    //--------------------------------------------------------------------
    // data setup
    //--------------------------------------------------------------------
    xhl::aligned_vector<int> vector_out(mat_i.num_rows);
    xhl::aligned_vector<int> adj_data(mat_i.adj_data.size());
    xhl::aligned_vector<int> adj_indices(mat_i.adj_indices.size());
    xhl::aligned_vector<int> adj_indptr(mat_i.adj_indptr.size());
    std::copy(mat_i.adj_data.begin(), mat_i.adj_data.end(), adj_data.begin());
    std::copy(mat_i.adj_indices.begin(), mat_i.adj_indices.end(), adj_indices.begin());
    std::copy(mat_i.adj_indptr.begin(), mat_i.adj_indptr.end(), adj_indptr.begin());

    //--------------------------------------------------------------------
    // Profiling Setup
    //--------------------------------------------------------------------
    TIMER_INIT(time);
    Measure compute_time;

    //--------------------------------------------------------------------
    // Compute Unit Setup
    //--------------------------------------------------------------------
    std::cout << "INFO : SpMV 3 Iterations Test" << std::endl;
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
    std::vector<xhl::Device> devices = xhl::find_devices(
        xhl::boards::alveo::u280::identifier
    );
    xhl::Device device = devices[0];
    device.program_device(argv[1]);   

    xhl::ComputeUnit spmv_cu(spmv);
    spmv_cu.bind(&device);
    
    device.create_buffer(
        "values", (adj_data.size()) * sizeof(int), adj_data.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[0]
    );
    device.create_buffer(
        "col_idx", (adj_indices.size()) * sizeof(int), adj_indices.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
    );
    device.create_buffer(
        "row_ptr", (adj_indptr.size()) * sizeof(int), adj_indptr.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[2]
    );
    device.create_buffer(
        "vector_in", (vector_in.size()) * sizeof(int), vector_in.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[3]
    );
    device.create_buffer(
        "vector_out", (vector_out.size()) * sizeof(int), vector_out.data(), 
        xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[4]
    );

    xhl::sync_data_htod(&device, "values");
    xhl::sync_data_htod(&device, "col_idx");
    xhl::sync_data_htod(&device, "row_ptr");
    xhl::sync_data_htod(&device, "vector_in");

    for (int i = 0; i < N; i++) {
        TIME_IT(time) {
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
            if (i != N-1) {
                xhl::sync_data_dtoh(&device, "vector_out");
                std::copy(vector_out.begin(), vector_out.end(), vector_in.begin());
                xhl::sync_data_htod(&device, "vector_in");
            }
        }
        compute_time.addSample(time);
    }
    xhl::sync_data_dtoh(&device, "vector_out");

    //--------------------------------------------------------------------
    // compare result
    //--------------------------------------------------------------------
    bool pass = check_buffer(vector_out, ref_result);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    std::cout << "\t\tTotal\t\tAvg\t\tMin\t\tMax" << std::endl;
    std::cout << "Compute:\t" << compute_time << std::endl;
    
    std::cout << "INFO : SpMV kernel complete!" << std::endl;

    return 0;
}