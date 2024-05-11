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
#include "sparse-io.hpp"

#include "profiling-infra.h"

#include "xcl2.hpp"
// check buffer
bool check_results(
    xhl::aligned_vector<float> v,
    std::vector<float> ref,
    bool stop_on_mismatch = true,
    float tolerance = 1e-3
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
        if (std::abs(v[i] - ref[i]) > tolerance) {
            std::cout << "[ERROR]: Value mismatch!" << std::endl;
            std::cout << "         at i = " << i << std::endl;
            printf("         Expected: %0.8f\n", ref[i]);
            printf("         Actual  : %0.8f\n", v[i]);
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
    CSRMatrix<float> &mat,
    std::vector<float> &vector,
    std::vector<float> &ref_result
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
    CSRMatrix<float> &mat,
    xhl::aligned_vector<float> &vector,
    std::vector<float> &ref_result,
    size_t iterations
) {
    ref_result.resize(mat.num_rows);
    std::vector<float> vec0(vector.begin(), vector.end());
    std::vector<float> vec1(vector.size());
    for (size_t i = 0; i < iterations; i++)
        compute_ref(mat, i%2==0 ? vec0 : vec1, i%2==0 ? vec1 : vec0);
    std::copy(
        (iterations%2==0 ? vec0 : vec1).begin(),
        (iterations%2==0 ? vec0 : vec1).end(),
        ref_result.begin()
    );
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
    CSRMatrix<float> mat = load_csr_matrix_from_float_npz(argv[2]);

    //--------------------------------------------------------------------
    // generate input vector
    //--------------------------------------------------------------------
    xhl::aligned_vector<float> vector_in(mat.num_cols);
    std::generate(
        vector_in.begin(),
        vector_in.end(),
        [&](){return (float)rand() / (float)(RAND_MAX/10);}
    );

    //--------------------------------------------------------------------
    // compute reference
    //--------------------------------------------------------------------
    std::vector<float> ref_result;
    compute_ref(mat, vector_in, ref_result, N);
    std::cout << "INFO : Compute reference complete!" << std::endl;

    //--------------------------------------------------------------------
    // data setup
    //--------------------------------------------------------------------
    xhl::aligned_vector<float> vector_out(mat.num_rows);
    xhl::aligned_vector<float> adj_data(mat.adj_data.size());
    xhl::aligned_vector<unsigned> adj_indices(mat.adj_indices.size());
    xhl::aligned_vector<unsigned> adj_indptr(mat.adj_indptr.size());
    std::copy(mat.adj_data.begin(), mat.adj_data.end(), adj_data.begin());
    std::copy(mat.adj_indices.begin(), mat.adj_indices.end(), adj_indices.begin());
    std::copy(mat.adj_indptr.begin(), mat.adj_indptr.end(), adj_indptr.begin());

    //--------------------------------------------------------------------
    // Profiling Setup
    //--------------------------------------------------------------------
    TIMER_INIT(time);
    Measure compute_time;

    //--------------------------------------------------------------------
    // Compute Unit Setup
    //--------------------------------------------------------------------
    std::cout << "INFO : SpMV " << N << " Iterations Test" << std::endl;
    xhl::KernelSignature spmv = {
        "spmv", {
            {"values", "float*"},
            {"col_idx", "unsigned*"},
            {"row_ptr", "unsigned*"},
            {"vector_in", "float*"},
            {"vector_out", "float*"},
            {"num_rows", "unsigned"},
            {"num_cols", "unsigned"}
        }
    };
    std::vector<xhl::Device> devices = xhl::find_devices(
        xhl::boards::alveo::u280::identifier
    );
    xhl::Device device = devices[0];
    device.program_device(argv[1]);

    xhl::ComputeUnit* spmv_cu = device.find(spmv);

    device.create_buffer(
        "values", (adj_data.size()) * sizeof(float), adj_data.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[0]
    );
    device.create_buffer(
        "col_idx", (adj_indices.size()) * sizeof(unsigned), adj_indices.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
    );
    device.create_buffer(
        "row_ptr", (adj_indptr.size()) * sizeof(unsigned), adj_indptr.data(),
        xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
    );
    device.create_buffer(
        "vector_in", (vector_in.size()) * sizeof(float), vector_in.data(),
        xhl::BufferType::ReadWrite, xhl::boards::alveo::u280::HBM[2]
    );
    device.create_buffer(
        "vector_out", (vector_out.size()) * sizeof(float), vector_out.data(),
        xhl::BufferType::ReadWrite, xhl::boards::alveo::u280::HBM[2]
    );

    xhl::sync_data_htod(&device, "values");
    xhl::sync_data_htod(&device, "col_idx");
    xhl::sync_data_htod(&device, "row_ptr");
    xhl::sync_data_htod(&device, "vector_in");

    for (int i = 0; i < N; i++) {
        TIME_IT(time) {
            spmv_cu->launch(
                device.get_buffer("values"),
                device.get_buffer("col_idx"),
                device.get_buffer("row_ptr"),
                (i % 2) ? device.get_buffer("vector_out") : device.get_buffer("vector_in"),
                (i % 2) ? device.get_buffer("vector_in") : device.get_buffer("vector_out"),
                mat.num_rows,
                mat.num_cols
            );
            device.finish_all_tasks();
        }
        compute_time.addSample(time);
    }
    if (N % 2 == 0) {
        xhl::sync_data_dtoh(&device, "vector_in");
        std::copy(vector_in.begin(), vector_in.end(), vector_out.begin());
    } else {
        xhl::sync_data_dtoh(&device, "vector_out");
    }

    //--------------------------------------------------------------------
    // compare result
    //--------------------------------------------------------------------
    bool pass = check_results(vector_out, ref_result);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    std::cout << "\t\tTotal\t\tAvg\t\tMin\t\tMax" << std::endl;
    std::cout << "Compute:\t" << compute_time << std::endl;

    std::cout << "INFO : SpMV kernel complete!" << std::endl;

    delete spmv_cu;

    return 0;
}
