#ifndef XOCL_HOSTLIB
#define XOCL_HOSTLIB

#include "xcl2/xcl2.hpp"
#include <vector>
#include "xocl-host-lib.hpp"
#include <unordered_map>

namespace xhl {
    ExecMode check_execution_mode() {
        if(xcl::is_emulation()) {
            if(xcl::is_hw_emulation())
                return ExecMode::HW_EMU;
            return ExecMode::SW_EMU;
        }
        return ExecMode::HW;
    }

    // runtime namespace, only happens on run time
    namespace runtime {
            void Device::create_buffer(
                std::string name, void *data_ptr, size_t size_in_bytes, BufferType type,
                const int memory_channel_name 
            ) {
                cl_mem_ext_ptr_t eptr;
                eptr.flags = memory_channel_name;
                eptr.obj = data_ptr;
                eptr.param = 0;
                this->_ext_ptrs[name] = eptr;
                switch (type) {
                    case ReadOnly:
                        this->buffers[name] = cl::Buffer(
                            this->_context,
                            CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                            size_in_bytes,
                            &(this->_ext_ptrs[name]),
                            &(this->_errflag)
                        );
                        if (this->_errflag) {
                            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                        }
                    break;
                    case WriteOnly:
                        this->buffers[name] = cl::Buffer(
                            this->_context,
                            CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                            size_in_bytes,
                            &(this->_ext_ptrs[name]),
                            &(this->_errflag)
                        );
                        if (this->_errflag) {
                            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                        }
                    break;
                    case ReadWrite:
                        this->buffers[name] = cl::Buffer(
                            this->_context,
                            CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                            size_in_bytes,
                            &(this->_ext_ptrs[name]),
                            &(this->_errflag)
                        );
                        if (this->_errflag) {
                            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                        }
                    break;
                    default:
                        std::cout << "[ERROR]: Unsupported buffer type!"<< std::endl;
                    break;
                }
            }

            void Device::program_device(
                const std::string &xclbin_path
            ) {
                // load bitstream into FPGA
                cl::Context cxt = cl::Context(this->_device, NULL, NULL, NULL);
                this->_context = cxt;

                const std::string bitstream_file_name = xclbin_path;
                std::vector<unsigned char> file_buf = xcl::read_binary_file(bitstream_file_name);
                cl::Program::Binaries binary{{file_buf.data(), file_buf.size()}};

                cl_int err = 0;
                cl::Program program(cxt, {this->_device}, binary, NULL, &err);
                if (err != CL_SUCCESS) {
                    throw std::runtime_error("[ERROR]: Load bitstream failed, exit!\n");
                }
            }
        // end of Device function

        std::vector<Device> find_devices(const xhl::BoardIdentifier &identifier) {
            // find device, push found device into our devices
            std::vector<Device> devices;
            std::string target_name =
                (check_execution_mode() == ExecMode::HW)
                ? identifier.xsa : identifier.name;
            bool found_device = false;
            auto cl_devices = xcl::get_xil_devices();
            for (size_t i = 0; i < cl_devices.size(); i++) {
                if (cl_devices[i].getInfo<CL_DEVICE_NAME>() == target_name) {
                    Device tmp_device(cl_devices[i]); // use special constructor function
                    devices.push_back(tmp_device);
                    found_device = true;
                }
            }
            if (!found_device) {
                throw std::runtime_error("[ERROR]: Failed to find Device, exit!\n");
            }
            return devices;
        }
    };

    void ComputeUnit::bind (xhl::runtime::Device *d) {
        this->cu_device = d;
        // create CL kernel in compute unit
        cl_int errflag = 0;
        this->clkernel = cl::Kernel(
            this->cu_device->_program, this->cu_kernel->ks.name.c_str(), &errflag
        );
        if (errflag) {
            throw std::runtime_error("[ERROR]: Failed to create CL Kernel, exit!\n");
        }
    }

    template <typename... Ts>
    void ComputeUnit::launch (Ts ... ts) {
        this->cu_kernel->run(this, ts...); 
        this->cu_device->command_q.enqueueTask(this->clkernel);
    }

    template<typename T>
    void Kernel::set_arg(
        const std::string& arg_name, 
        const xhl::ComputeUnit *cu,
        const T&arg_val
    ) {
        cl_int errflag = 0;

        // use iterator to find the match between the input argument name and the argument map in kernel signature
        // argmap maps from argument name to argument type
        std::map<std::string, std::string>::iterator it;
        int i = 0;
        for (it = cu->cu_kernel->ks.argmap.begin(); it != cu->cu_kernel->ks.argmap.end() ; ++it) {
            if (it -> first == arg_name) {
                errflag = cu->clkernel.setArg(i, arg_val);
                throw std::runtime_error("[ERROR]: Failed to setArg in CL Kernel, exit!\n");
                break;
            }
            i++;
        }
        // check if match
        if (i == cu->cu_kernel->ks.argmap.size()) {
            throw std::runtime_error("[ERROR]: input argument name doesn't match with the argument map(map from argument name to argument type), exit!\n");
        }
    }

};

#endif //XOCL_HOSTLIB
