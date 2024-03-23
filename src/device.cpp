#include "device.hpp"

namespace xhl {

void Device::bind_device(cl::Device cl_device) {
    this->_device = cl_device;
    // std::cout << *this << std::endl;
}

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

std::string Device::name() {
    return this->_device.getInfo<CL_DEVICE_NAME>();
}


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
            (tmp_device).bind_device(cl_devices[i]); // use function
            devices.push_back(tmp_device);
            found_device = true;
        }
    }
    if (!found_device) {
        throw std::runtime_error("[ERROR]: Failed to find Device, exit!\n");
    }
    return devices;
}

void Device::finish_all_tasks() {
    cl_int err = this->command_q.finish();
    if (err != CL_SUCCESS) {
        throw std::runtime_error(
            "[ERROR]: Failed to finish all tasks, exit! (code:"
            + std::to_string(err) + ")"
        );
    }
}

} // namespace xhl