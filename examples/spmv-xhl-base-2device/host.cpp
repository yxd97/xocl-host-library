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
#include "link.hpp"
#include "data_loader.h"
#include "host_memory_link.h"

#include "timer.h"

#include "xcl2.hpp"

typedef unsigned IDX_T;
std::vector<spmv::io::CSRMatrix<int>> partitionMatrixIn2(
    spmv::io::CSRMatrix<int> matrix,
    bool balance_workload = false
) {
    std::vector<spmv::io::CSRMatrix<int>> partitions;
    uint32_t row_partition[2] = {matrix.num_rows / 2, matrix.num_rows - matrix.num_rows / 2};
    if (balance_workload) {
        size_t half = matrix.adj_indices.size() / 2;
        size_t ind;
        for (ind = 0; ind < matrix.adj_indptr.size(); ind++)
            if (matrix.adj_indptr[ind] > half)
                break;
        if (ind != 0 && half-matrix.adj_indptr[ind-1] < matrix.adj_indptr[ind] - half)
            --ind;
        row_partition[0] = ind;
        row_partition[1] = matrix.num_rows - ind;
    }
    uint32_t start = 0;
    for (int p = 0; p < 2; p++) {
        spmv::io::CSRMatrix<int> part;
        part.num_cols = matrix.num_cols;
        part.num_rows = row_partition[p];
        uint32_t end = start + part.num_rows;
        part.adj_indptr.push_back(0);
        for(uint32_t i = start; i < end; i++) {
            for (uint32_t j = matrix.adj_indptr[i]; j < matrix.adj_indptr[i+1]; j++) {
                part.adj_indices.push_back(matrix.adj_indices[j]);
                part.adj_data.push_back(matrix.adj_data[j]);
            }
            part.adj_indptr.push_back(part.adj_data.size());
        }
        partitions.push_back(part);
        start += row_partition[p];
    }
    return partitions;
}

bool check_buffer(
    std::vector<int> v,
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
    std::vector<int> &vector,
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
        std::cout << "Usage : " << argv[0] << " <xclbin path> <dataset path> [balance_workload]" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 1;
    }
    bool balance_workload = false;
    if (argc > 3) {
        if (strcmp(argv[3], "true") == 0)
            balance_workload = true;
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
    std::vector<spmv::io::CSRMatrix<int>> pmat = partitionMatrixIn2(mat_i, balance_workload);
    
    //--------------------------------------------------------------------
    // generate input vector
    //--------------------------------------------------------------------
    std::vector<int> vector_i(mat_i.num_cols);
    std::generate(
        vector_i.begin(), 
        vector_i.end(), 
        [&](){return int(rand() % 2);}
    );

    //--------------------------------------------------------------------
    // compute reference
    //--------------------------------------------------------------------
    std::vector<int> ref_result;
    compute_ref(mat_i, vector_i, ref_result, N);
    std::cout << "INFO : Compute reference complete!" << std::endl;

    //--------------------------------------------------------------------
    // data setup
    //--------------------------------------------------------------------
    std::vector<xhl::aligned_vector<int>> vector_in_vec;
    std::vector<xhl::aligned_vector<int>> vector_out_vec;
    std::vector<xhl::aligned_vector<int>> adj_data_vec;
    std::vector<xhl::aligned_vector<int>> adj_indices_vec;
    std::vector<xhl::aligned_vector<int>> adj_indptr_vec;
    for (int i = 0; i < 2; i++) {
        vector_in_vec.push_back(xhl::aligned_vector<int>(pmat[i].num_cols));
        vector_out_vec.push_back(xhl::aligned_vector<int>(pmat[i].num_rows));
        adj_data_vec.push_back(xhl::aligned_vector<int>(pmat[i].adj_data.size()));
        adj_indices_vec.push_back(xhl::aligned_vector<int>(pmat[i].adj_indices.size()));
        adj_indptr_vec.push_back(xhl::aligned_vector<int>(pmat[i].adj_indptr.size()));
        std::copy(vector_i.begin(), vector_i.end(), vector_in_vec[i].begin());
        std::copy(pmat[i].adj_data.begin(), pmat[i].adj_data.end(), adj_data_vec[i].begin());
        std::copy(pmat[i].adj_indices.begin(), pmat[i].adj_indices.end(), adj_indices_vec[i].begin());
        std::copy(pmat[i].adj_indptr.begin(), pmat[i].adj_indptr.end(), adj_indptr_vec[i].begin());
    }

    //--------------------------------------------------------------------
    // Profiling Setup
    //--------------------------------------------------------------------
    TIMER_INIT(time);
    Measure compute_time, communicate_time;

    //--------------------------------------------------------------------
    // Compute Unit Setup
    //--------------------------------------------------------------------
    std::cout << "INFO : Distributed SpMV 3 Iterations Test (" << (balance_workload?"balanced":"not balanced") << ")" << std::endl;
    const xhl::KernelSignature spmv = {
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
    if (devices.size() < 2) {
        std::cout << "This example requires 2 devices, " << devices.size() << " found." << std::endl;
        return 1;
    }

    std::vector<xhl::ComputeUnit> cus;
    for (int i = 0; i < 2; i++) {
        xhl::Device &device = devices[i];
        device.program_device(argv[1]);
        xhl::ComputeUnit spmv_cu(spmv);
        spmv_cu.bind(&device);
        cus.push_back(spmv_cu);

        device.create_buffer(
            "values", (pmat[i].adj_data.size()) * sizeof(int), adj_data_vec[i].data(),
            xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[0]
        );
        device.create_buffer(
            "col_idx", (pmat[i].adj_indices.size()) * sizeof(int), adj_indices_vec[i].data(),
            xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[1]
        );
        device.create_buffer(
            "row_ptr", (pmat[i].adj_indptr.size()) * sizeof(int), adj_indptr_vec[i].data(),
            xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[2]
        );
        device.create_buffer(
            "vector_in", (pmat[i].num_cols) * sizeof(int), vector_in_vec[i].data(),
            xhl::BufferType::ReadOnly, xhl::boards::alveo::u280::HBM[3]
        );
        device.create_buffer(
            "vector_out", (pmat[i].num_rows) * sizeof(int), vector_out_vec[i].data(),
            xhl::BufferType::WriteOnly, xhl::boards::alveo::u280::HBM[4]
        );

        xhl::nb_sync_data_htod(&device, "values");
        xhl::nb_sync_data_htod(&device, "col_idx");
        xhl::nb_sync_data_htod(&device, "row_ptr");
        xhl::nb_sync_data_htod(&device, "vector_in");
    }
    for (int j = 0; j < 2; j++)
        devices[j].finish_all_tasks();
        
    std::unique_ptr<xhl::Link> link_00, link_11, link_01, link_10;

    link_00 = std::make_unique<HostMemoryLink>(&devices[0], &devices[0]);
    link_11 = std::make_unique<HostMemoryLink>(&devices[1], &devices[1]);
    link_01 = std::make_unique<HostMemoryLink>(&devices[0], &devices[1]);
    link_10 = std::make_unique<HostMemoryLink>(&devices[1], &devices[0]);
    
    //--------------------------------------------------------------------
    // Compute Unit Launch
    //--------------------------------------------------------------------
    for (int i = 0; i < N; i++) {
        TIME_IT(time) {
            for (int j = 0; j < 2; j++) {
                cus[j].launch(
                    devices[j].get_buffer("values"),
                    devices[j].get_buffer("col_idx"),
                    devices[j].get_buffer("row_ptr"),
                    devices[j].get_buffer("vector_in"),
                    devices[j].get_buffer("vector_out"),
                    pmat[j].num_rows,
                    pmat[j].num_cols
                );
            }
            for (int j = 0; j < 2; j++)
                devices[j].finish_all_tasks();
        }
        compute_time.addSample(time);

        if (i != N-1) {
            TIME_IT(time) {
                link_00->transfer("vector_out", "vector_in", 0, 0, pmat[0].num_rows * sizeof(int));
                link_01->transfer("vector_out", "vector_in", 0, 0, pmat[0].num_rows * sizeof(int));
                link_11->transfer("vector_out", "vector_in", 0, pmat[0].num_rows * sizeof(int), pmat[1].num_rows * sizeof(int));
                link_10->transfer("vector_out", "vector_in", 0, pmat[0].num_rows * sizeof(int), pmat[1].num_rows * sizeof(int));
            }
            communicate_time.addSample(time);
        }
    }
    
    for (xhl::Device device : devices)
        xhl::nb_sync_data_dtoh(&device, "vector_out");
    for (xhl::Device device : devices)
        device.finish_all_tasks();

    //--------------------------------------------------------------------
    // compare result
    //--------------------------------------------------------------------
    std::vector<int> vector_o(mat_i.num_rows);
    std::copy(vector_out_vec[0].begin(), vector_out_vec[0].end(), vector_o.begin());
    std::copy(vector_out_vec[1].begin(), vector_out_vec[1].end(), vector_o.begin()+pmat[0].num_rows);
    bool pass = check_buffer(vector_o, ref_result);
    std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;
    std::cout << "\t\tTotal\t\tAvg\t\tMin\t\tMax" << std::endl;
    std::cout << "Compute:\t" << compute_time << std::endl;
    std::cout << "Communicate:\t" << communicate_time << std::endl;
    
    std::cout << "INFO : SpMV kernel complete!" << std::endl;

    return 0;
}