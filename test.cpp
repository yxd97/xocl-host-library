// #include "devices.h"
// #include "runtime.h"

#include "vvadd.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>
#include <algorithm>
#include <xocl-host-lib.hpp>
// #include "xcl2.hpp"
#include <cstdlib>   // For rand() function
#include <ctime>     // For srand() function
// #define output_size 5
#define num_rows 5
// #define num_cols 5
using namespace std;
using namespace xhl;
template<typename T>
using aligned_vector = std::vector<T, aligned_allocator<T> >;
//--------------------------------------------------------------------------------------------------
// Utilities
//--------------------------------------------------------------------------------------------------
bool check_aligned_vector(
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
void generateRandomCSR(int numRows, int numCols, int density, aligned_vector<int> &row_ptr, aligned_vector<int> &col_idx, aligned_vector<int> &values) {
    // Seed for random number generation
    srand(0);

    // Initialize the CSR matrix
    row_ptr.push_back(0);

    // Loop through each row
    for (int i = 0; i < numRows; ++i) {
        // Loop through each column in the current row
        for (int j = 0; j < numCols; ++j) {
            // Generate a random number to determine if the element is non-zero
            if (rand() % 100 < density) {
                // If the element is non-zero, add it to the matrix
                col_idx.push_back(j);
                values.push_back(static_cast<int>(rand() % 100)); // You can modify this for different value ranges
            }
        }

        // Update the row_ptr vector with the number of non-zero elements so far
        row_ptr.push_back(static_cast<int>(col_idx.size()));
    }
}
void printCSR(int numRows, const aligned_vector<int> &row_ptr, const aligned_vector<int> &col_idx, const aligned_vector<int> &values) {
    cout << "Row Pointer: ";
    for (int i = 0; i <= numRows; ++i) {
        cout << row_ptr[i] << " ";
    }
    cout << "\nColumn Index: ";
    for (size_t i = 0; i < col_idx.size(); ++i) {
        cout << col_idx[i] << " ";
    }
    cout << "\nValues: ";
    for (size_t i = 0; i < values.size(); ++i) {
        cout << values[i] << " ";
    }
    cout << endl;
}
aligned_vector<int> matrixVectorMultiply(const aligned_vector<int> &row_ptr, const aligned_vector<int> &col_idx, const aligned_vector<int> &values, const aligned_vector<int> &vector) {
    int numRows = row_ptr.size() - 1;
    aligned_vector<int> result(numRows, 0.0);

    for (int i = 0; i < numRows; ++i) {
        for (int j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            result[i] += values[j] * vector[col_idx[j]];
        }
    }

    return result;
}
//--------------------------------------------------------------------------------------------------
// Testbench
//--------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    // parse arguments
    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << "<xclbin path>" << std::endl;
        std::cout << "Aborting..." << std::endl;
        return 0;
    }
    std::string xclbin_path = argv[1];

    // set the environment variable
    ExecMode target = check_execution_mode();
    cout << target << endl;
    if (target == SW_EMU) {
        setenv("XCL_EMULATION_MODE", "sw_emu", true);
    } else if (target == HW_EMU) {
        setenv("XCL_EMULATION_MODE", "hw_emu", true);
    }
    int numRows = output_size;
    int numCols = num_cols;
    int density = 30;  // Percentage density of non-zero elements

    aligned_vector<int> row_ptr;
    aligned_vector<int> col_idx;
    aligned_vector<int> values;
    aligned_vector<int> vector_out;

    generateRandomCSR(numRows, numCols, density, row_ptr, col_idx, values);
    // printCSR(numRows, row_ptr, col_idx, values);
    aligned_vector<int> inputVector(numCols);
    for (int i = 0; i < numCols; ++i) {
        inputVector[i] = static_cast<int>(rand() % 100);
    }
    aligned_vector<int> result = matrixVectorMultiply(row_ptr, col_idx, values, inputVector);

    // Print the input vector
    cout << "\nInput Vector: ";
    for (int i = 0; i < numCols; ++i) {
        cout << inputVector[i] << " ";
    }
    cout << "\n";

    // Print the result vector
    cout << "Result Vector: ";
    for (int i = 0; i < numRows; ++i) {
        cout << result[i] << " ";
    }
    cout << "\n";
    vector_out.resize(result.size(), 0);
    cl::Device cl_device;
    runtime::Device rt(cl_device);
    rt.program_device(xclbin_path);
    // std::vector<xhl::runtime::Device> devices = xhl::runtime::find_devices(runtime::boards::alveo::u280::identifier);


    // KernelSignature kernel = {
    //     "spmv", {
    //         {"matrix", 0},
    //         {"vector_in", 1},
    //         {"vector_out", 2},
    //         {"row_partition_id", 3},
    //         {"partition_num_rows", 4},
    //         {"num_col_partitions", 5},
    //         {"total_num_partitions", 6}
    //     }
    // };
    // // std::unordered_map<std::string, int> map{{"in1", 0}, {"in2", 1}, {"out", 2}, {"DATA_SIZE", 3},{"row_ptr",4},{"vector_out",5},{"output_size",6},{"col_idx",7},{"values",8},{"inputVector",9},{"num_cols",10}};mod
    // std::unordered_map<std::string, int> map{{"row_ptr",0},{"vector_out",1},{"col_idx",2},{"values",3},{"inputVector",4}};
    // std::vector<std::unordered_map<std::string, int>> kernel_arg_map(1, map);
    // rt.create_kernels({"vvadd"}, kernel_arg_map);

    // // generate inputs/outputs
    // // aligned_vector<int> in1(DATA_SIZE);
    // // aligned_vector<int> in2(DATA_SIZE);
    // // aligned_vector<int> out_ref(DATA_SIZE);
    // aligned_vector<int> out(DATA_SIZE);
    // // int output_size=out_ref.size();
    // // int values[]={10, 20, 30, 4, 50, 60, 70, 80};
    // // std::generate(
    // //     in1.begin(), in1.end(),
    // //     [](){return rand() % 10;}
    // // );
    // // std::generate(
    // //     in2.begin(), in2.end(),
    // //     [](){return rand() % 10;}
    // // );
    // // for (size_t i = 0; i < out_ref.size(); i++) {
    // //     out_ref[i] = in1[i] + in2[i];
    // // }
    // // std::cout <<"soeifjiowef"<< row_ptr.data() << std::endl;
    // // create buffers
    // // rt.create_buffer(
    // //     "in1", in1.data(), sizeof(int) * DATA_SIZE, BufferType::ReadOnly, alveo::u280::HBM[0]
    // // );mod
    // // rt.create_buffer(
    // //     "in2", in2.data(), sizeof(int) * DATA_SIZE, BufferType::ReadOnly, alveo::u280::HBM[1]
    // // );
    // rt.create_buffer(
    //     "row_ptr", row_ptr.data(), row_ptr.size()*sizeof(int), BufferType::ReadOnly, alveo::u280::HBM[1]
    // );
    // rt.create_buffer(
    //     "col_idx", col_idx.data(), col_idx.size()*sizeof(int), BufferType::ReadOnly, alveo::u280::HBM[1]
    // );
    // rt.create_buffer(
    //     "values", values.data(), values.size()*sizeof(int), BufferType::ReadOnly, alveo::u280::HBM[1]
    // );
    // rt.create_buffer(
    //     "inputVector", inputVector.data(), inputVector.size()*sizeof(int), BufferType::ReadOnly, alveo::u280::HBM[1]
    // );
    // // rt.create_buffer(
    // //     "out", out.data(), sizeof(int) * DATA_SIZE, BufferType::WriteOnly, alveo::u280::HBM[2]
    // // );
    // rt.create_buffer(
    //     "vector_out", vector_out.data(), vector_out.size()*sizeof(int), BufferType::WriteOnly, alveo::u280::HBM[2]
    // );
    // // migrate data
    // // rt.migrate_data({"in1", "in2","row_ptr","col_idx","values","inputVector"}, ToDevice);//mod
    // // rt.migrate_data({"in2","row_ptr","col_idx","values","inputVector"}, ToDevice);//mod
    // rt.migrate_data({"row_ptr","col_idx","values","inputVector"}, ToDevice);//mod
    // // set kernel arguments
    // cl_int err;
    // // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(0, rt.buffers["in1"]));//mod
    // // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(0, rt.buffers["in2"]));
    // // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(1, rt.buffers["out"]));
    // // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(2, DATA_SIZE));
    // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(0, rt.buffers["row_ptr"]));
    // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(1, rt.buffers["vector_out"]));
    // // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(2, output_size));
    // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(2, rt.buffers["col_idx"]));
    // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(3, rt.buffers["values"]));
    // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(4, rt.buffers["inputVector"]));
    // // OCL_CHECK(err, err = rt.kernels["vvadd"].setArg(6, num_cols));
    // // run the kernel!
    // rt.command_q.enqueueTask(rt.kernels["vvadd"]);
    // rt.command_q.finish();

    // // collect results
    // // rt.migrate_data({"out"}, ToHost);
    // rt.migrate_data({"vector_out"}, ToHost);
    // for (int i = 0; i < int(vector_out.size()); ++i) {
    //     std::cout << vector_out[i] << " ";
    // }
    // // verify
    // // bool pass = check_aligned_vector(out, out_ref);
    // // std::cout << (pass ? "[INFO]: Test Passed !" : "[ERROR]: Test Failed!") << std::endl;

    return 0;
}