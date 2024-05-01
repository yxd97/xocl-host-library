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
    virtual ~IBuffer() = default;
    virtual const cl::Buffer buffer() const = 0;
    virtual void* data() = 0;
    virtual const size_t size_in_bytes() const = 0;
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
    std::string name;
protected:
    cl_mem_ext_ptr_t _eptr;
    cl::Buffer* _buffer;
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
        size_t size
    )  : _buffer(nullptr), _data(size) {}

    void set_clbuffer(cl::Buffer buf) {
        *_buffer = buf;
    }
public:

    T& operator[](size_t idx) { return _data[idx]; }
    const T& operator[](size_t idx) const { return _data[idx]; }
    auto begin() { return this->_data.begin(); }
    auto begin() const { return this->_data.begin(); }
    auto end() { return this->_data.end(); }
    auto end() const { return this->_data.end(); }
    size_t size() const { return this->_data.size(); }

    const cl::Buffer buffer() const override { return *_buffer; }
    void* data() {
        return this->_data.data();
    }
    const size_t size_in_bytes() const {
        return this->_data.size() * sizeof(T);
    }

    const xhl::aligned_vector<T> vector() const { return _data; }
};
} // namespace xhl
#endif
