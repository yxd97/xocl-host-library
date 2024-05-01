#include "device.hpp"
#include "compute_unit.hpp"
#include <vector>

namespace xhl {

void Device::create_buffer(
    std::string name, size_t size, void* data_ptr, BufferType type,
    const int memory_channel_name
) {
    if (this->_buffers.find(name) != this->_buffers.end()) {
        throw std::runtime_error("Buffer name already used");
    }
    cl_mem_flags buffer_type_flag;
    switch (type) {
        case ReadOnly:
            buffer_type_flag = CL_MEM_READ_ONLY;
        break;
        case WriteOnly:
            buffer_type_flag = CL_MEM_WRITE_ONLY;
        break;
        case ReadWrite:
            buffer_type_flag = CL_MEM_READ_WRITE;
        break;
        default:
            throw std::runtime_error("Invalid buffer type");
        break;
    }
    this->_ext_ptrs[name].flags = memory_channel_name;
    this->_ext_ptrs[name].obj = data_ptr;
    this->_ext_ptrs[name].param = 0;
    cl_int err = 0;
    this->_buffers[name] = cl::Buffer(
        this->_context,
        buffer_type_flag | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
        size,
        &(this->_ext_ptrs[name]),
        &(err)
    );
    if (err) {
        throw std::runtime_error(
            "Creation of underlying cl::Buffer failed"
            " with error code " + std::to_string(err)
        );
    }
}

void Device::bind_device(cl::Device cl_device) {
    this->_device = cl_device;
    // std::cout << *this << std::endl;
}

bool Device::contains_buffer(const std::string &name){
    return this->_buffers.find(name) != this->_buffers.end();
}

cl::Buffer Device::get_buffer(const std::string &name) {
    return this->_buffers[name];
}

void Device::program_device(
    const std::string &xclbin_path
) {
    cl_int err = 0;

    // load bitstream into FPGA
    this->_context = cl::Context(this->_device, NULL, NULL, NULL);;

    const std::string bitstream_file_name = xclbin_path;
    std::vector<unsigned char> file_buf = xcl::read_binary_file(bitstream_file_name);
    cl::Program::Binaries binary{{file_buf.data(), file_buf.size()}};

    err = 0;
    this->program = cl::Program(this->_context, {this->_device}, binary, NULL, &(err));
    if (err != CL_SUCCESS) {
        throw std::runtime_error("[ERROR]: Load bitstream failed, exit!\n");
    }

    err = 0;
    this->command_q = cl::CommandQueue(
        this->_context, this->_device,
        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE,
        &(err)
    );
    if (err != CL_SUCCESS) {
        throw std::runtime_error("[ERROR]: Failed to create command queue, exit!\n");
    }
}

ComputeUnit* Device::find(const KernelSignature &signature) {
    // create the computeunit depending on signature, bind it to the device
    // and return the computeunit
    ComputeUnit* cu = new ComputeUnit(signature);
    cu->bind(this);
    return cu;
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
            std::cout << "device size: " << cl_devices.size() << std:: endl;
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

void nb_sync_data_htod(xhl::Device* device, const std::string &buffer_name) {
    cl_int err = device->command_q.enqueueMigrateMemObjects(
        {device->get_buffer(buffer_name)},
        0 /* 0 means from host */
    );
    if (err != CL_SUCCESS) {
        throw std::runtime_error(
            "Failed to migrate data for buffer to device (code:"
            + std::to_string(err) + ")"
        );
    }
}


void nb_sync_data_dtoh(xhl::Device* device, const std::string &buffer_name) {
    cl_int err = device->command_q.enqueueMigrateMemObjects(
        {device->get_buffer(buffer_name)},
        CL_MIGRATE_MEM_OBJECT_HOST
    );
    if (err != CL_SUCCESS) {
        throw std::runtime_error(
            "Failed to migrate data for buffer to device (code:"
            + std::to_string(err) + ")"
        );
    }
}


void sync_data_htod(xhl::Device* device, const std::string &buffer_name) {
    nb_sync_data_htod(device, buffer_name);
    device->command_q.finish();
}


void sync_data_dtoh(xhl::Device* device, const std::string &buffer_name) {
    nb_sync_data_dtoh(device, buffer_name);
    device->command_q.finish();
}

} // namespace xhl