#include "link.hpp"

namespace xhl {
void* get_data_ptr(Device* device, const std::string &buffer_name) {
    void* data;
    cl_int err;
    err = device->get_buffer(buffer_name).getInfo(CL_MEM_HOST_PTR, &data);
    if (err != CL_SUCCESS)
        throw std::runtime_error("Failed to get buffer data for " + buffer_name + " (code:" + std::to_string(err) +")");
    return data;
}
size_t get_size(Device* device, const std::string &buffer_name) {
    size_t size;
    cl_int err;
    err = device->get_buffer(buffer_name).getInfo(CL_MEM_SIZE, &size);
    if (err != CL_SUCCESS)
        throw std::runtime_error("Failed to get buffer size for " + buffer_name + " (code:" + std::to_string(err) +")");
    return size;
} // namespace xhl
}