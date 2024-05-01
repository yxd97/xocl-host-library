#include "utils.hpp"

namespace xhl {


ExecMode check_execution_mode() {
    if(xcl::is_emulation()) {
        if(xcl::is_hw_emulation())
            return ExecMode::HW_EMU;
        return ExecMode::SW_EMU;
    }
    return ExecMode::HW;
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

} // namespace xhl