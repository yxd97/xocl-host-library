#ifndef XOCL_HOSTLIB_RUNTIME_H
#define XOCL_HOSTLIB_RUNTIME_H

#include <unordered_map>
#include "xcl2/xcl2.hpp"
#include "types.hpp"

ExecMode check_execution_mode() {
    if(xcl::is_emulation()) {
        if(xcl::is_hw_emulation())
            return ExecMode::HW_EMU;
        return ExecMode::SW_EMU;
    }
    return ExecMode::HW;
}

// RunTime class: helps to manage kernels and buffers
class RunTime {
private:
    cl::Device _device;
    cl::Program _program;
    cl_int _errflag;
    std::unordered_map<std::string, cl_mem_ext_ptr_t> _ext_ptrs;

public:
    cl::Context context;
    cl::CommandQueue command_q;
    std::unordered_map<std::string, cl::Kernel> kernels;
    std::unordered_map<std::string, ArgumentMap> arg_maps;
    std::unordered_map<std::string, cl::Buffer> buffers;

    /**
     * @brief program the fpga with the xclbin
     *
     * @param board_identifier the board identifier
     * @param xclbin_path path to the xclbin file
     */
    void program_device(
        const BoardIdentifier &board_identifier,
        const std::string &xclbin_path
    ) {
        // find device
        std::string target_name =
            (check_execution_mode() == ExecMode::HW)
            ? board_identifier.xsa : board_identifier.name;
        bool found_device = false;
        auto devices = xcl::get_xil_devices();
        for (size_t i = 0; i < devices.size(); i++) {
            if (devices[i].getInfo<CL_DEVICE_NAME>() == target_name) {
                this->_device = devices[i];
                found_device = true;
                break;
            }
        }
        if (!found_device) {
            std::cout << "[ERROR]: Failed to find " << target_name << ", exit!" << std::endl;
            exit(EXIT_FAILURE);
        }

        // setup run time context
        this->context = cl::Context(this->_device, NULL, NULL, NULL);

        // read FPGA binary
        auto file_buf = xcl::read_binary_file(xclbin_path);
        cl::Program::Binaries binaries{{file_buf.data(), file_buf.size()}};
        OCL_CHECK(
            this->_errflag, this->_program = cl::Program(
                context, {this->_device}, binaries, NULL, &(this->_errflag)
            )
        );

        // create command queue
        OCL_CHECK(
            this->_errflag,
            this->command_q = cl::CommandQueue(
                    this->context,
                    this->_device,
                    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE,
                    &(this->_errflag)
            )
        );
    }

    /**
     * @brief Create multiple instances of a kernel (specified by nk tags in the Vitis build command)
     *
     * @param kernel_signature function signature of the kernel
     * @param instance_names the names of the instances as a vector of strings
     */
    void create_kernel_instances(
        const KernelSignature &kernel_signature,
        const std::vector<std::string> &instance_names
    ) {
        for (size_t i = 0; i < instance_names.size(); i++) {
            std::string instance_path = kernel_signature.name + ":{" + instance_names[i] + "}";
            OCL_CHECK(
                this->_errflag,
                this->kernels[instance_names[i]] = cl::Kernel(
                    this->_program, instance_path.c_str(), &(this->_errflag)
                )
            );
            this->arg_maps[instance_names[i]] = kernel_signature.argmap;
        }
    }

    /**
     * @brief Create a kernel
     *
     * @param kernel_signature function signature of the kernel
     */
    void create_kernel(
        const KernelSignature &kernel_signature
    ) {
        OCL_CHECK(
            this->_errflag,
            this->kernels[kernel_signature.name] = cl::Kernel(
                this->_program, kernel_signature.name.c_str(), &(this->_errflag)
            )
        );
        this->arg_maps[kernel_signature.name] = kernel_signature.argmap;
    }

    /**
     * @brief Set the kernel arg object
     *
     * @tparam argument type
     * @param instance_name
     * @param arg_name
     * @param arg_val
     */
    template<typename T>
    void set_kernel_arg(
        const std::string& instance_name,
        const std::string& arg_name,
        const T& arg_val
    ) {
        // std::cout << "Setting kernel arg " << arg_name << " for instance " << instance_name << std::endl;
        OCL_CHECK(
            this->_errflag,
            this->_errflag = this->kernels[instance_name].setArg(
                this->arg_maps[instance_name][arg_name],
                arg_val
            )
        );
    }

    // memory management functions
    void create_buffer(
        std::string name, void *data_ptr, size_t size_in_bytes, BufferType type,
        const int memory_channel_id
    ) {
        cl_mem_ext_ptr_t eptr;
        eptr.flags = memory_channel_id;
        eptr.obj = data_ptr;
        eptr.param = 0;
        this->_ext_ptrs[name] = eptr;
        switch (type) {
            case ReadOnly:
                OCL_CHECK(
                    this->_errflag, this->buffers[name] = cl::Buffer(
                        this->context,
                        CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                        size_in_bytes,
                        &(this->_ext_ptrs[name]),
                        &(this->_errflag)
                    );
                )
            break;
            case WriteOnly:
                OCL_CHECK(
                    this->_errflag, this->buffers[name] = cl::Buffer(
                        this->context,
                        CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                        size_in_bytes,
                        &(this->_ext_ptrs[name]),
                        &(this->_errflag)
                    );
                )
            break;
            case ReadWrite:
                OCL_CHECK(
                    this->_errflag, this->buffers[name] = cl::Buffer(
                        this->context,
                        CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                        size_in_bytes,
                        &(this->_ext_ptrs[name]),
                        &(this->_errflag)
                    );
                )
            break;
            default:
                std::cout << "[ERROR]: Unsupported buffer type!"<< std::endl;
            break;
        }
    }

    // data transfer functions
    void nb_migrate_data(std::vector<std::string> buffer_list, MigrateDirection direction) {
        switch (direction) {
            case ToDevice:
                for (auto bn : buffer_list) {
                    this->_errflag = this->command_q.enqueueMigrateMemObjects(
                        {this->buffers[bn]},
                        0
                    );
                    if (this->_errflag != CL_SUCCESS) {
                        throw std::runtime_error(
                            "Failed to migrate data for buffer " + bn + " to device!"
                        );
                    }
                }
            break;
            case ToHost:
                for (auto bn : buffer_list) {
                    this->_errflag = this->command_q.enqueueMigrateMemObjects(
                        {this->buffers[bn]},
                        CL_MIGRATE_MEM_OBJECT_HOST
                    );
                    if (this->_errflag != CL_SUCCESS) {
                        throw std::runtime_error(
                            "Failed to migrate data for buffer " + bn + " to host!"
                        );
                    }
                }
            break;
            default:
                throw std::runtime_error("Unsupported migration direction!");
            break;
        }
    }

    void migrate_data(std::vector<std::string> buffer_list, MigrateDirection direction) {
        this->nb_migrate_data(buffer_list, direction);
        this->command_q.finish();
    }




};

#endif //XOCL_HOSTLIB_RUNTIME_H
