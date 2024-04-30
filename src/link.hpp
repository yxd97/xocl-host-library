#pragma once

#include "xcl2.hpp"
#include "device.hpp"
#include <memory>

namespace xhl {
class Link {
    protected:
        Link() = default;
    public:
        /**
         * @brief Transfers `n` bytes from the head of the source buffer to the head of
         * the destination buffer. `n` is the smaller of the lengths of the two buffers.
         * 
         * @param src_buffer name of the source buffer
         * @param dst_buffer name of the destination buffer
         * 
         * @throws `std::runtime_error` if data could not be transmitted
        */
        virtual void transfer(const std::string &src_buffer, const std::string &dst_buffer) = 0;

        /**
         * @brief Transfers `byte_count` number of bytes from the source buffer, at the `src_offset`,
         * to the destination buffer, at the `dst_offset`.
         * 
         * @param src_buffer name of the source buffer
         * @param dst_buffer name of the destination buffer
         * 
         * @throws `std::runtime_error` if data could not be transmitted
        */
        virtual void transfer(const std::string &src_buffer, const std::string &dst_buffer, 
                                size_t src_offset, size_t dst_offset, size_t byte_count) = 0;
};

void* get_data_ptr(Device* device, const std::string &buffer_name);
size_t get_size(Device* device, const std::string &buffer_name);
}