#include <iostream>
#include <string>
#include <vector>

#include "devices.hpp"
#include "runtime.hpp"
#include "types.hpp"

#include "sp_data_structure.h"

const unsigned NUM_PE = 8;
const unsigned PACK_SIZE = NUM_PE;
const unsigned VB_SIZE = 1024;
const unsigned OB_SIZE = 1024 * NUM_PE;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <xclbin> [dataset]" << std::endl;
        return 1;
    }
    std::string dataset = "";
    if (argc >= 3) {
        dataset = argv[2];
    }
    const std::string xclbin = argv[1];
    std::cout << " * Using xclbin: " << xclbin << std::endl;

    switch (check_execution_mode()) {
    case SW_EMU:
        std::cout << " * Running in software emulation mode" << std::endl;
        break;
    case HW_EMU:
        std::cout << " * Running in hardware emulation mode" << std::endl;
        break;
    case HW:
        std::cout << " * Running in hardware mode" << std::endl;
        break;
    default:
        std::cerr << " * Unknown execution mode" << std::endl;
        return 1;
    }

    RunTime runtime;
    runtime.program_device(alveo::u280::identifier, xclbin);

    KernelSignature kernel = {
        "spmv", {
            {"matrix", 0},
            {"vector_in", 1},
            {"vector_out", 2},
            {"row_partition_id", 3},
            {"partition_num_rows", 4},
            {"num_col_partitions", 5},
            {"total_num_partitions", 6}
        }
    };

    runtime.create_kernel(kernel);

    // initialize matrix
    std::cout << " * Initializing matrix" << std::endl;
    spmv::io::CSRMatrix<int> matrix;
    if (dataset != "") {
        std::cout << "  ** Load dataset " << dataset << std::endl;
        matrix.load_from_txt(dataset);
    } else {
        std::cout << "  ** Create uniform-16 512x512 as default" << std::endl;
        matrix.num_rows = 8192;
        matrix.num_cols = 8192;
        matrix.uniform_fill(16, 1);
    }
    std::cout << "  ** Round matrix dimensions:" << std::endl;
    std::cout << "   *** row divisor: " << 2*NUM_PE << std::endl;
    std::cout << "   *** col divisor: " << 2*PACK_SIZE << std::endl;
    matrix.round_dim(2*NUM_PE, 2*PACK_SIZE);
    std::cout << "  ** Partition matrix:" << std::endl;
    std::cout << "   *** row partition size: " << OB_SIZE << std::endl;
    std::cout << "   *** col partition size: " << VB_SIZE << std::endl;
    spmv::io::PartitionedCSR<int> partitioned_matrix(matrix, OB_SIZE, VB_SIZE);
    std::cout << "  ** Create CPSR matrix" << std::endl;
    spmv::io::CPSRMatrix<int, PACK_SIZE> cpsr_matrix(partitioned_matrix, 1);
    std::cout << " * done." << std::endl;

    // initialize vectors
    std::cout << " * Initializing vectors" << std::endl;
    std::vector<int> vector_in(matrix.num_cols, 1);
    std::vector<int> vector_out(matrix.num_rows, 0);

    // create memory layouts for matrix
    std::cout << " * Creating memory layouts for matrix" << std::endl;
    std::vector<spmv::io::channel_word_t<int, PACK_SIZE>> formatted_matrix;
    formatted_matrix.insert(
        formatted_matrix.end(),
        cpsr_matrix.partition_tables[0].begin(),
        cpsr_matrix.partition_tables[0].end()
    );
    formatted_matrix.insert(
        formatted_matrix.end(),
        cpsr_matrix.channel_data[0].begin(),
        cpsr_matrix.channel_data[0].end()
    );

    // create device buffers
    std::cout << " * Creating device buffers" << std::endl;
    runtime.create_buffer(
        "matrix",
        formatted_matrix.data(),
        formatted_matrix.size() * sizeof(spmv::io::channel_word_t<int, PACK_SIZE>),
        ReadOnly,
        alveo::u280::HBM[0]
    );
    runtime.create_buffer(
        "vector_in",
        vector_in.data(),
        vector_in.size() * sizeof(int),
        ReadOnly,
        alveo::u280::HBM[1]
    );
    runtime.create_buffer(
        "vector_out",
        vector_out.data(),
        vector_out.size() * sizeof(int),
        WriteOnly,
        alveo::u280::HBM[1]
    );

    // set kernel arguments
    std::cout << " * Setting kernel arguments" << std::endl;
    runtime.set_kernel_arg("spmv", "matrix", runtime.buffers["matrix"]);
    runtime.set_kernel_arg("spmv", "vector_in", runtime.buffers["vector_in"]);
    runtime.set_kernel_arg("spmv", "vector_out", runtime.buffers["vector_out"]);
    runtime.set_kernel_arg("spmv", "row_partition_id", 0);
    runtime.set_kernel_arg("spmv", "partition_num_rows", std::min(matrix.num_rows, OB_SIZE));
    runtime.set_kernel_arg("spmv", "num_col_partitions", cpsr_matrix.num_col_partitions);
    runtime.set_kernel_arg("spmv", "total_num_partitions", cpsr_matrix.num_col_partitions * cpsr_matrix.num_row_partitions);

    // move data to device
    std::cout << " * Moving data to device" << std::endl;
    runtime.migrate_data({"matrix", "vector_in"}, ToDevice);

    // run kernel
    std::cout << " * Running kernel" << std::endl;
    runtime.command_q.enqueueTask(runtime.kernels["spmv"]);
    runtime.command_q.finish();
    std::cout << "  ** done." << std::endl;

    // get results
    std::cout << " * Getting results" << std::endl;
    runtime.migrate_data({"vector_out"}, ToHost);
    std::cout << "  ** done." << std::endl;


    return 0;
}