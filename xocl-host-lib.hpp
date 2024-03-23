#ifndef XOCL_HOSTLIB_H
#define XOCL_HOSTLIB_H

#include "xcl2/xcl2.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#include <any>

namespace xhl {
    template <typename T>
    using aligned_vector = std::vector<T, aligned_allocator<T>>;

    // config words
    enum MigrateDirection {ToDevice, ToHost};
    enum BufferType {ReadOnly, WriteOnly, ReadWrite};
    enum ExecMode {SW_EMU, HW_EMU, HW};

    /**
     * @brief check the execution mode (SW, HW_EMU, HW)
     * @return return xhl::enmu type, including SW_EMU, HW_EMU, HW
     */
    ExecMode check_execution_mode();

    struct BoardIdentifier {
        std::string name;
        std::string xsa;
    };
    struct KernelSignature {
        // from argument name to argument type, string to string
        std::string name; // kernel name
        std::map<std::string, std::string> argmap; // argument map
    };

    namespace runtime {
        namespace boards {
            namespace alveo {
                #define CHANNEL_NAME(n) n | XCL_MEM_TOPOLOGY
                namespace u280 {
                    // instantiate u280 identifier
                    const static BoardIdentifier identifier = {
                        "xilinx_u280_gen3x16_xdma_1_202211_1",
                        "xilinx_u280_gen3x16_xdma_base_1"
                    };
                    // u280 memory channels
                    const int HBM[32] = {
                        CHANNEL_NAME(0),  CHANNEL_NAME(1),  CHANNEL_NAME(2),  CHANNEL_NAME(3),  CHANNEL_NAME(4),
                        CHANNEL_NAME(5),  CHANNEL_NAME(6),  CHANNEL_NAME(7),  CHANNEL_NAME(8),  CHANNEL_NAME(9),
                        CHANNEL_NAME(10), CHANNEL_NAME(11), CHANNEL_NAME(12), CHANNEL_NAME(13), CHANNEL_NAME(14),
                        CHANNEL_NAME(15), CHANNEL_NAME(16), CHANNEL_NAME(17), CHANNEL_NAME(18), CHANNEL_NAME(19),
                        CHANNEL_NAME(20), CHANNEL_NAME(21), CHANNEL_NAME(22), CHANNEL_NAME(23), CHANNEL_NAME(24),
                        CHANNEL_NAME(25), CHANNEL_NAME(26), CHANNEL_NAME(27), CHANNEL_NAME(28), CHANNEL_NAME(29),
                        CHANNEL_NAME(30), CHANNEL_NAME(31)
                    };
                    const int DDR[2] = {CHANNEL_NAME(32), CHANNEL_NAME(33)};
                };
            };
        };
        
        template <typename T>
        class Buffer; // Forward Declaration
        class Device {
            template <typename T>
            friend class Buffer;
            friend std::vector<Device> find_devices(const xhl::BoardIdentifier &identifier);
            private:
                cl::Device _device;
                std::unordered_map<std::string, cl_mem_ext_ptr_t> _ext_ptrs;
                cl::Context _context;
                cl_int _errflag;
                void bind_Device(cl::Device cl_device) {
                    _device = cl_device;
                    std::cout << *this << std::endl;
                }
            public:
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
                std::string name, size_t count, BufferType type,
                const int memory_channel_name
            ) {
                Buffer<T> buf = Buffer<T>(this, name, count, type, memory_channel_name);
                this->buffers[name] = buf._buffer;
                return buf;
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
            bool contains_buffer(cl::Buffer& buf);
            /**
             * @brief Determines if this `xhl::runtime::Device` contains the provided `Buffer`
             * 
             * @param buf the `Buffer` to check
             * 
             * @returns true is `buf` is part of this device, and false otherwise
             * 
             * @throws `std::runtime_exception` if `buf`'s owner device cannot be verified
            */
            template <typename U>
            bool contains_buffer(Buffer<U>& buf) const {
                return this->contains_buffer(buf._buffer);
            }
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

            friend std::ostream& operator<<(std::ostream& stream, const Device& device);
        };
        class IBuffer { // Interface Buffer
            public:
                virtual const cl::Buffer buffer() const = 0;
        };
        template <typename T>
        class Buffer : public IBuffer {
            friend class Device;
            public:
                Device* const device;
                std::string name;
            protected:
                cl_mem_ext_ptr_t _eptr;
                cl::Buffer _buffer;
                xhl::aligned_vector<T> _data;
                Buffer(Device* const device, std::string name, size_t count, BufferType type,
                    const int memory_channel_name) : device(device), name(name), _data(count) {
                    _eptr.flags = memory_channel_name;
                    _eptr.obj = (void*)_data.data();
                    _eptr.param = 0;
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
                    _buffer = cl::Buffer(
                        device->_context,
                        buffer_type_flag | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                        sizeof(T) * count,
                        &(_eptr),
                        &(device->_errflag)
                    );
                    if (device->_errflag) {
                        throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                    }
                }
            public:
                void nb_migrate_data(xhl::MigrateDirection dir) {
                    this->device->_errflag = this->device->command_q.enqueueMigrateMemObjects(
                        {this->_buffer}, dir==xhl::MigrateDirection::ToDevice?0:1);
                    if (this->device->_errflag != CL_SUCCESS) {
                        throw std::runtime_error(
                            "Failed to migrate data for buffer '" + this->name + "' to " +
                            (dir==xhl::MigrateDirection::ToDevice?"device":"host") +
                            " (code:" + std::to_string(this->device->_errflag) + ")"
                        );
                    }
                }
                void migrate_data(xhl::MigrateDirection dir) {
                    this->nb_migrate_data(dir);
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
        
        /**
         * @brief find the user needed device
         *
         * @param identifier is hardly defined in xhl::runtime::board::alveo::u280 namespace
         * @return the array of xhl::runtime::Device class instances
         * 
         * @exception Failed to find Device
         */
        std::vector<Device> find_devices(const xhl::BoardIdentifier&);
    };

    class ComputeUnit {
        private:
            template<typename T>
            void set_arg(
                const int arg_index,
                const T&arg_val
            ) {
                cl_int errflag = 0;
                if constexpr (std::is_base_of_v<runtime::IBuffer, T>)
                {
                    const runtime::IBuffer* ptr = &arg_val;
                    //if (ptr = static_cast<const runtime::IBuffer*>(&arg_val))
                    errflag = this->clkernel.setArg(arg_index, ptr->buffer());
                    //throw std::runtime_error("Should not be reached");
                }
                else
                    errflag = this->clkernel.setArg(arg_index, arg_val);
                if (errflag != CL_SUCCESS)
                    throw std::runtime_error("[ERROR]: Failed to setArg in CL Kernel, exit! (code:" + std::to_string(errflag) + ")");
            }
            // https://stackoverflow.com/a/71148253
            template <typename ...Ts, size_t ...Is>
            void invoke_at_impl(std::tuple<Ts...>& tpl, std::index_sequence<Is...>, size_t idx) {
                ((void)(Is == idx && (this->set_arg(idx, std::get<Is>(tpl)), true)), ...);
            }
            template <typename ...Ts>
            void invoke_at(std::tuple<Ts...>& tpl, size_t idx) {
                invoke_at_impl(tpl, std::make_index_sequence<sizeof...(Ts)>{}, idx);
            }
        public:
            xhl::runtime::Device *cu_device;
            struct xhl::KernelSignature signature;
            cl::Kernel clkernel;

            ComputeUnit() = delete; // don't provide default constructor 

            /**
             * @brief constructor instantiate the xhl::kernel into the ComputeUnit
             *
             * @param Kernel class under xhl namespace
             */
            ComputeUnit(struct xhl::KernelSignature ks) : cu_device(nullptr) { // add xhl::kernel and assign it to computeunit.
                this->signature = ks;
            }

            /**
             * @brief bind the device with the computeunit and create the cl::Kernel, which is the computeunit in our define
             *
             * @param Device class under xhl::runtime namespace
             * 
             * @exception Failed to create CL Kernel
             */
            void bind (xhl::runtime::Device *d);

            /**
             * @brief set argument in clkernel
             *
             * @param argument name
             * @param argument content in any type corresponding to the name
             * 
             * @exception Failed to set argument
             * @exception Failed to match any argument name in map
             */
            template<typename T>
            void set_arg(
                const std::string& arg_name,
                const T&arg_val
            );

            /**
             * @brief launch, start to run the computeunit
             *
             * @param param pack, can send any number of param in any type
             */
            template <typename... Ts>
            void launch (Ts ... ts) {
                if (sizeof...(Ts) < this->signature.argmap.size())
                    throw std::runtime_error("Too few arguments supplied to compute unit launch");
                if (sizeof...(Ts) > this->signature.argmap.size())
                    throw std::runtime_error("Too many arguments supplied to compute unit launch");
                
                auto args = std::make_tuple(ts...);
                auto ite = this->signature.argmap.begin();
                for (int i = 0; i < this->signature.argmap.size(); i++, ite++) {
                    invoke_at(args, i);
                }
                this->cu_device->command_q.enqueueTask(this->clkernel);
            }
    };
};

#endif //XOCL_HOSTLIB_H