#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <vector>
#include <unordered_map>
#include <iostream>
#include <string>

#include "xcl2.hpp"

#include "xocl-host-lib.hpp"

namespace xhl {
class ComputeUnit;
class Device {

private:
cl::Device _device;
cl::Context _context;

std::unordered_map<std::string, cl_mem_ext_ptr_t> _ext_ptrs;
std::unordered_map<std::string, cl::Buffer> _buffers;

public:
cl::CommandQueue command_q; // used to queue tasks
cl::Program program; // used to create kernel

/**
 * @brief
 *
 * @param cl_device
 */
void bind_device(cl::Device cl_device);

/**
 * @brief create buffer depending on the buffertype
 *
 * @param name the argument name
 * @param size the size of the buffer in bytes
 * @param data_ptr the pointer to the data
 * @param type the BufferType: ReadOnly, WriteOnly, and ReadWrite
 * @param memory_channel_name should be an int from an array in of a particular board under xhl::boards
 * (e.g., xhl::boards::alveo::u280::DDR[0])
 *
 * @exception std::runtime_error if the underlying xocl call fails
 * @exception std::runtime_error if the buffer type is wrong
 * @exception std::runtime_error if the buffer name is already used
 */
void create_buffer(
    std::string name, size_t size, void* data_ptr, BufferType type,
    const int memory_channel_name
);


/**
 * @brief Determines if this `xhl::runtime::Device` contains a buffer with the provided name
 *
 * @param name the name of the buffer to check
 * @return true if the buffer is part of this device, and false otherwise
 */
bool contains_buffer(const std::string &name);


/**
 * @brief get the buffer with the provided name
 *
 * @param name the name of the buffer to get
 * @return the buffer with the provided name
 */
cl::Buffer get_buffer(const std::string &name);


/**
 * @brief use bitstream to program the device. Create the compute units,
 * return all existing compute units, including the new ones.
 *
 * @param xclbin_path the path of bitstream/xclbin
 * @param signatures the kernel signatures
 *
 * @exception fail to load bitstream
 */
void program_device(
    const std::string &xclbin_path
);

ComputeUnit* find(const KernelSignature &signature);

/**
 * @brief get the name of the device
 *
 * @return [std::string] the name of the device
 */
std::string name();

/**
 * @brief wait until all tasks to finish
 * @throws std::runtime_error when fail to finish all tasks
 */
void finish_all_tasks();

};


/**
 * @brief
 *
 * @param identifier
 * @return std::vector<Device>
 */
std::vector<Device> find_devices(const xhl::BoardIdentifier &identifier);

/**
 * @brief
 *
 * @param device
 * @param buffer_name
 */
void nb_sync_data_htod(Device* device, const std::string &buffer_name);

/**
 * @brief
 *
 * @param device
 * @param buffer_name
 */
void nb_sync_data_dtoh(Device* device, const std::string &buffer_name);

/**
 * @brief
 *
 * @param device
 * @param buffer_name
 */
void sync_data_htod(Device* device, const std::string &buffer_name);

/**
 * @brief
 *
 * @param device
 * @param buffer_name
 */
void sync_data_dtoh(Device* device, const std::string &buffer_name);

} // namespace xhl

#endif // DEVICE_HPP