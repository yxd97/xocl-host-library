#include "device.hpp"
#include "link.hpp"

#include <algorithm>

#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <functional>

class HostMemoryLink : public xhl::Link {
    private:
        xhl::Device *const src_device, *const dst_device;
    public:
        HostMemoryLink(xhl::Device* src_device, xhl::Device* dst_device)
            : src_device(src_device), dst_device(dst_device) {}
        void transfer(const std::string &src_buffer, const std::string &dst_buffer) override {
            try {
                xhl::nb_sync_data_dtoh(src_device, src_buffer);
                size_t src_size = xhl::get_size(src_device, src_buffer);
                size_t dst_size = xhl::get_size(dst_device, dst_buffer);
                void *src_data  = xhl::get_data_ptr(src_device, src_buffer);
                void *dst_data  = xhl::get_data_ptr(dst_device, dst_buffer);
                src_device->command_q.finish();

                std::copy((uint8_t*)src_data,
                          (uint8_t*)src_data+std::min(src_size, dst_size),
                          (uint8_t*)dst_data);
                
                xhl::sync_data_htod(dst_device, dst_buffer);
            }
            catch (const std::exception& e) {
                throw std::runtime_error("Failed to transfer data from " + src_buffer +
                    " to " + dst_buffer + " because of\n" + e.what());
            }
        }
        void wait() override {}
        void finish() override {}
};

// https://stackoverflow.com/a/4793662
class bsem { // Binary Semaphore
    std::mutex mutex_;
    std::condition_variable condition_;
    bool free;
public:
    bsem() : free(false) {}
    bsem(bool free) : free(free) {}
    void release() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        free = true;
        condition_.notify_one();
    }

    void acquire() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!free) // Handle spurious wake-ups.
            condition_.wait(lock);
        free = false;
    }

    bool try_acquire() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        if(free) {
            free = false;
            return true;
        }
        return false;
    }
};

using namespace boost::asio::ip;
class HostTCPLink : public xhl::Link, public std::enable_shared_from_this<HostTCPLink> {
    private:
        xhl::Device *const src_device;
        const std::string _target_hostname;
        const uint16_t _port;
        boost::asio::io_context _io_context;
        tcp::socket _socket;
        bool connected;

        bsem _socket_sem, _run_sem, _wait_sem;
        std::thread _handler_thread, _listen_thread;


        uint8_t recv_buffer[4096];
        char name_arr[1024];
        size_t name_length = 0, remaining_length = 0, recv_buffer_offset = 0;

        std::string buffer_name = std::string();
        uint8_t* data_buffer_ptr = nullptr;
        size_t data_buffer_size = 0;
        size_t data_buffer_offset = 0;
        void do_read(bool loop = false) {
            // std::cout << (void*)this << "-" << "read:Acquiring socket" << std::endl;
            if (!loop) _socket_sem.acquire();

            this->_socket.async_read_some(
                boost::asio::buffer(recv_buffer+recv_buffer_offset, 4096-recv_buffer_offset),
                [&](boost::system::error_code err, std::size_t size) {
                    size += recv_buffer_offset;
                    // std::cout << (void*)this << "-" << "Received " + std::to_string(size) + " bytes" << std::endl;
                    size_t i = 0;
                    while (i < size) {
                        if (remaining_length == 0) {
                            switch(recv_buffer[i]) {
                                case 0:
                                    _wait_sem.release();
                                    i++;
                                break;
                                case 1:
                                    // Collect data until at least 9 bytes is avaliable
                                    if (size < sizeof(size_t) + 1) {
                                        recv_buffer_offset = size;
                                        continue;
                                    }
                                    recv_buffer_offset = 0;

                                    remaining_length = *((size_t*)(recv_buffer+i+1)) - sizeof(size_t) - 1;
                                    name_length = 0;
                                    i += sizeof(size_t) + 1;
                                    // std::cout << (void*)this << "-" << "Packet Length: " << remaining_length + 5 << std::endl;
                                break;
                                default:
                                    throw std::runtime_error("Invalid byte received");
                                break;
                            }
                        }
                        else if (buffer_name.empty()) {
                            // Collect chars in 0 terminated string for buffer name
                            char c = (char)recv_buffer[i++];
                            name_arr[name_length++] = c;
                            --remaining_length;
                            if (c == '\0') {
                                buffer_name = std::string(name_arr);
                                // std::cout << (void*)this << "-" << "Buffer Name: " << buffer_name << std::endl;
                                name_length = 0;
                                data_buffer_ptr = (uint8_t*)xhl::get_data_ptr(this->src_device, buffer_name);
                                data_buffer_size = xhl::get_size(this->src_device, buffer_name);
                                data_buffer_offset = 0;
                            }
                        }
                        else {
                            // Copy data into buffer_data_ptr
                            size_t len = std::min(remaining_length, size-i);
                            std::copy(recv_buffer+i,
                                        recv_buffer+i+std::min(len, data_buffer_size-data_buffer_offset),
                                        data_buffer_ptr+data_buffer_offset);
                            data_buffer_offset += len;
                            remaining_length -= len;
                            i += len;
                            if (remaining_length == 0)
                            {
                                //std::cout << (void*)this << "-" << "Begin Data Migrate: " << buffer_name << std::endl;
                                xhl::nb_sync_data_htod(this->src_device, buffer_name);
                                buffer_name = std::string();
                            }
                        }
                    }

                    if (!err) {
                        // std::cout << (void*)this << "-" << "read:Next Async Read" << std::endl;
                        this->do_read(true);
                    }
                    else {
                        // std::cout << (void*)this << "-" << "read:Releasing socket" << std::endl;
                        _socket_sem.release();
                        if (err == boost::asio::error::operation_aborted) {
                            // std::cout << (void*)this << "-" << "Read Cancelled" << std::endl;
                        }
                    }
                }
            );
            // std::cout << (void*)this << "-" << "Read Began" << std::endl;
            
            _run_sem.release();
        }
    public:
        HostTCPLink(xhl::Device* src_device, const std::string &target_hostname, uint16_t port, bool listen)
            : src_device(src_device), _target_hostname(target_hostname), _port(port),
            _io_context(), _socket(_io_context), connected(false),
            _socket_sem(true), _run_sem(false), _wait_sem(false) {
            if (listen) {
                tcp::acceptor a(_io_context, tcp::endpoint(tcp::v4(), this->_port));
                this->_socket = a.accept();
                this->connected = true;
                this->do_read(false);
            }
            else {
                tcp::resolver resolver(_io_context);
                tcp::resolver::results_type endpoints = resolver.resolve(_target_hostname, std::to_string(_port));
                boost::asio::connect(this->_socket, endpoints);
                this->connected = true;
                this->do_read(false);
            }
            _handler_thread = std::thread([&]{ 
                while (this->connected) {
                    _run_sem.acquire();
                    // std::cout << (void*)this << "-" << "io_context.restart()" << std::endl;
                    _io_context.restart();
                    // std::cout << (void*)this << "-" << "io_context.run() start" << std::endl;
                    _io_context.run();
                    // std::cout << (void*)this << "-" << "io_context.run() finish" << std::endl;
                }
            });
        }
        ~HostTCPLink() {
            if (this->connected) {
                this->_socket.cancel();
                this->_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                this->_socket.close();
            }
        }
        void transfer(const std::string &src_buffer, const std::string &dst_buffer) override {
            try {
                if (dst_buffer.size() > 1024)
                    throw std::runtime_error("Buffer name longer than 1024 characters not supported");
                if (!this->connected)
                    throw std::runtime_error("TCP connection disconnected");
                xhl::nb_sync_data_dtoh(src_device, src_buffer);
                size_t data_buffer_size = xhl::get_size(src_device, src_buffer);
                void* data_buffer_ptr = xhl::get_data_ptr(src_device, src_buffer);

                // std::cout << (void*)this << "-" << "transfer:Canceling read" << std::endl;
                this->_socket.cancel();
                // std::cout << (void*)this << "-" << "transfer:Acquiring socket" << std::endl;
                _socket_sem.acquire();
                uint8_t send_buffer[1024+sizeof(size_t)+1];
                *send_buffer = 1;
                *((size_t*)(send_buffer+1)) = 1 + sizeof(size_t) + dst_buffer.size() + 1 + data_buffer_size;
                std::copy(dst_buffer.cbegin(), dst_buffer.cend(), send_buffer+sizeof(size_t)+1);
                send_buffer[1+sizeof(size_t)+dst_buffer.size()] = '\0';
                boost::asio::write(this->_socket, boost::asio::buffer(send_buffer, 1 + sizeof(size_t) + dst_buffer.size() + 1));
                src_device->command_q.finish();
                boost::asio::write(this->_socket, boost::asio::buffer((uint8_t*)data_buffer_ptr, data_buffer_size));
                // std::cout << (void*)this << "-" << "transfer:Releasing socket" << std::endl;
                _socket_sem.release();
                this->do_read(false);

                // std::cout << (void*)this << "-" << "Sent data from " + src_buffer + " to " + dst_buffer << std::endl;
            }
            catch (const std::exception& e) {
                throw std::runtime_error("Failed to transfer data from " + src_buffer +
                    " to " + dst_buffer + " because of\n" + e.what());
            }
        }
        void wait() override {
            _wait_sem.acquire();
            this->src_device->command_q.finish();
        }
        void finish() override {
            // std::cout << (void*)this << "-" << "finish:Canceling read" << std::endl;
            this->_socket.cancel();
            // std::cout << (void*)this << "-" << "finish:Acquiring socket" << std::endl;
            _socket_sem.acquire();
            uint8_t flag = 0;
            boost::asio::write(this->_socket, boost::asio::buffer(&flag, 1));
            // std::cout << (void*)this << "-" << "finish:Releasing socket" << std::endl;
            _socket_sem.release();
            this->do_read(false);

            // std::cout << (void*)this << "-" << "Sent finish" << std::endl;
        }
};