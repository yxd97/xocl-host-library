#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <vector>
#include <unordered_map>
#include <iostream>
#include <string>

#include "xcl2.hpp"

#include "buffer.hpp"
#include "xocl-host-lib.hpp"

namespace xhl {

template <typename T> class Buffer; // Forward declaration

class Device {

template <typename T>
friend class Buffer;

private:
cl::Device _device;
std::unordered_map<std::string, cl_mem_ext_ptr_t> _ext_ptrs;
cl::Context _context;

public:

/**
 * @brief
 *
 * @param cl_device
 */
void bind_device(cl::Device cl_device);

cl_int _errflag;
std::unordered_map<std::string, cl::Buffer> buffers;
cl::CommandQueue command_q;
cl::Program _program; // used to create kernel

/**
 * @brief create buffer depending on the buffertype
 *
 * @param name is the argument name
 * @param count is the number of elements of the buffer
 * @param type is the BufferType: ReadOnly, WriteOnly, and ReadWrite
 * @param memory_channel_name should be a int from an array in in a particular board under xhl::runtime::board
 *
 * @exception fail to create buffer
 * @exception wrong type buffer
 */
template <typename T>
Buffer<T> create_buffer(
    std::string name, size_t size, BufferType type,
    const int memory_channel_name
) {
    Buffer<T> buffer = Buffer<T>(size);
    buffer._eptr.flags = memory_channel_name;
    buffer._eptr.obj = buffer.data();
    buffer._eptr.param = 0;
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
            std::cout << "[ERROR]: Unsupported buffer type!"<< std::endl;
        break;
    }
    cl::Buffer buf = cl::Buffer(
        this->_context,
        buffer_type_flag | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
        sizeof(T) * size,
        &(buffer._eptr),
        &(this->_errflag)
    );
    if (this->_errflag) {
        throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
    }
    this->buffers[name] = buf;
    buffer.set_clbuffer(buf);
    return buffer;
}

/**
 * @brief Determines if this `xhl::runtime::Device` contains the provided `cl::Buffer`
 *
 * @param buf the `cl::Buffer` to check
 *
 * @returns true is `buf` is part of this device, and false otherwise
 *
 * @throws `std::runtime_exception` if `buf`'s owner device cannot be verified
*/
bool contains_buffer(cl::Buffer& buf) const;

/**
 * @brief
 *
 * @param buf
 * @return true
 * @return false
 */
bool contains_buffer(cl::Buffer& buf);

/**
 * @brief Determines if this `xhl::runtime::Device` contains the provided `Buffer`
 *
 * @param buf the `Buffer` to check
 * @tparam U the type of the `Buffer` to check
 *
 * @returns true is `buf` is part of this device, and false otherwise
 *
 * @throws `std::runtime_exception` if `buf`'s owner device cannot be verified
*/
template <typename U>
bool contains_buffer(Buffer<U>& buf) const {
    return this->contains_buffer(buf._buffer);
}

/**
 * @brief
 *
 * @tparam U
 * @param buf
 * @return true
 * @return false
 */
template <typename U>
bool contains_buffer(Buffer<U>& buf) {
    return this->contains_buffer(buf._buffer);

}

/**
 * @brief use bitstream to program the device
 *
 * @param the path of bitstream/xclbin
 *
 * @exception fail to load bitstream
 */
void program_device(const std::string &xclbin_path);

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


} // namespace xhl

#endif // DEVICE_HPP