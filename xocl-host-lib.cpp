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
        bool Device::contains_buffer(cl::Buffer& buf) const {
            cl_context bufCtx;
            cl_int err = buf.getInfo(CL_MEM_CONTEXT, &bufCtx);
            if (err != CL_SUCCESS) throw std::runtime_error("Unable to obtain buffer context");
            return bufCtx == this->_context();
        }
        bool Device::contains_buffer(cl::Buffer& buf) {
            cl_context bufCtx;
            cl_int err = buf.getInfo(CL_MEM_CONTEXT, &bufCtx);
            if (err != CL_SUCCESS) throw std::runtime_error("Unable to obtain buffer context");
            return bufCtx == this->_context();
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

            //cl_int err = 0;
            cl::Program program(cxt, {this->_device}, binary, NULL, &(this->_errflag));
            if (this->_errflag != CL_SUCCESS) {
                throw std::runtime_error("[ERROR]: Load bitstream failed, exit!\n");
            }
            this->_program = program;

            cl::CommandQueue cq(cxt, this->_device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE, &(this->_errflag));
            if (this->_errflag != CL_SUCCESS) {
                throw std::runtime_error("[ERROR]: Failed to create command queue, exit!\n");
            }
            this->command_q = cq;
        }

        std::ostream& operator<<(std::ostream& stream, const Device& device)
        {
            return stream << device._device.getInfo<CL_DEVICE_NAME>();
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
                    Device tmp_device;
                    (tmp_device).bind_Device(cl_devices[i]); // use function
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
            this->cu_device->_program, this->signature.name.c_str(), &errflag
        );
        if (errflag != CL_SUCCESS) {
            throw std::runtime_error("[ERROR]: Failed to create CL Kernel, exit! (code:" + std::to_string(errflag) + ")");
        }
    }

    template<typename T>
    void ComputeUnit::set_arg(
        const std::string& arg_name,
        const T&arg_val
    ) {
        cl_int errflag = 0;

        // use iterator to find the match between the input argument name and the argument map in kernel signature
        // argmap maps from argument name to argument type
        std::map<std::string, std::string>::iterator it;
        int i = 0;
        for (it = this->signature.argmap.begin(); it != this->signature.argmap.end() ; ++it) {
            if (it -> first == arg_name) {
                set_arg(i, arg_val);
            }
            i++;
        }
        // check if match
        if (i == this->signature.argmap.size()) {
            throw std::runtime_error("[ERROR]: input argument name doesn't match with the argument map(map from argument name to argument type), exit!\n");
        }
    }
};

#endif //XOCL_HOSTLIB