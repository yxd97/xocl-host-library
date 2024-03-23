#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <vector>

#include "xcl2.hpp"
#include "device.hpp"
#include "xocl-host-lib.hpp"

namespace xhl {

class Device; // Forward declaration

class IBuffer { // Interface Buffer
public:
    virtual const cl::Buffer buffer() const = 0;
};

/**
 * @brief
 *
 * @tparam T
 */
template <typename T>
class Buffer : public IBuffer {
friend class Device;

public:
    Device* device;
    std::string name;
protected:
    cl_mem_ext_ptr_t _eptr;
    cl::Buffer _buffer;
    std::vector<T, aligned_allocator<T>> _data;
    /**
     * @brief Construct a new Buffer object
     *
     * @param device
     * @param name
     * @param count
     * @param type
     * @param memory_channel_name
     */
    Buffer (
        Device* const device,
        std::string name,
        size_t count,
        BufferType type,
        const int memory_channel_name
    )  : device(device), name(name), _data(count) {
        this->_eptr.flags = memory_channel_name;
        this->_eptr.obj = (void*)_data.data();
        this->_eptr.param = 0;
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
        this->_buffer = cl::Buffer(
            this->device->_context,
            buffer_type_flag | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
            sizeof(T) * count,
            &(this->_eptr),
            &(this->device->_errflag)
        );
        if (this->device->_errflag) {
            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
        }
    }
public:
    /**
     * @brief Migrate data to the device
     */
    void nb_migrate_data(xhl::MigrateDirection dir) {
        this->device->_errflag = device->command_q.enqueueMigrateMemObjects(
            {this->_buffer},
            dir == ToDevice ? 0 : 1
        );
        if (this->device->_errflag != CL_SUCCESS) {
            throw std::runtime_error(
                "Failed to migrate data for buffer '" + name + "' to " +
                (dir == ToDevice ? "device" : "host") +
                " (code:" + std::to_string(device->_errflag) + ")"
            );
        }
    }

    /**
     * @brief
     *
     * @param dir
     */
    void migrate_data(xhl::MigrateDirection dir) {
        this->nb_migrate_data(dir);
        // TODO: add exception handling for finish()
        this->device->command_q.finish();
    }

    T& operator[](size_t idx) { return _data[idx]; }
    const T& operator[](size_t idx) const { return _data[idx]; }
    auto begin() { return this->_data.begin(); }
    auto begin() const { return this->_data.begin(); }
    auto end() { return this->_data.end(); }
    auto end() const { return this->_data.end(); }
    size_t size() const { return this->_data.size(); }

    const cl::Buffer buffer() const override { return _buffer; }
    const xhl::aligned_vector<T> vector() const { return _data; }
};
} // namespace xhl
#endif
