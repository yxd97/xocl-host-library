#include <algorithm>

#include "xocl-host-lib.hpp"
#include "link.hpp"
#include "device.hpp"

class HostMemoryLink : public xhl::Link {
    private:
        xhl::Device *const src_device, *const dst_device;
    public:
        HostMemoryLink(xhl::Device* src_device, xhl::Device* dst_device)
            : src_device(src_device), dst_device(dst_device) {}
        void transfer(const std::string &src_buffer, const std::string &dst_buffer) override {
            size_t src_size = xhl::get_size(src_device, src_buffer);
            size_t dst_size = xhl::get_size(dst_device, dst_buffer);
            transfer(src_buffer, dst_buffer, 0, 0, std::min(src_size, dst_size));
        }
        void transfer(const std::string &src_buffer, const std::string &dst_buffer,
            size_t src_offset, size_t dst_offset, size_t byte_count) override {
            try {
                xhl::nb_sync_data_dtoh(src_device, src_buffer);
                size_t src_size = xhl::get_size(src_device, src_buffer);
                size_t dst_size = xhl::get_size(dst_device, dst_buffer);
                void *src_data  = xhl::get_data_ptr(src_device, src_buffer);
                void *dst_data  = xhl::get_data_ptr(dst_device, dst_buffer);
                src_device->finish_all_tasks();

                if (src_offset+byte_count > src_size ||
                    dst_offset+byte_count > dst_size ||
                    src_offset < 0 ||
                    dst_offset < 0)
                    throw std::invalid_argument("Index out of bounds");

                std::copy((uint8_t*)src_data+src_offset,
                          (uint8_t*)src_data+src_offset+byte_count,
                          (uint8_t*)dst_data+dst_offset);
                
                xhl::sync_data_htod(dst_device, dst_buffer);
            }
            catch (const std::exception& e) {
                throw std::runtime_error("Failed to transfer data from " + src_buffer +
                    " to " + dst_buffer + " because of\n" + e.what());
            }
        }
};