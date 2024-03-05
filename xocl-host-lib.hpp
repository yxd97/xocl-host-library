#ifndef XOCL_HOSTLIB_H
#define XOCL_HOSTLIB_H

#include "xcl2/xcl2.hpp"
#include <vector>
#include <map>
#include <unordered_map>

namespace xhl{
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
        class Device {
            private:
                cl::Device _device;
                cl_int _errflag;
                cl::Context _context;
                std::unordered_map<std::string, cl_mem_ext_ptr_t> _ext_ptrs;
            public:
                std::unordered_map<std::string, cl::Buffer> buffers;
                cl::CommandQueue command_q;
                cl::Program _program; // used to create kernel

            // constructor
            // used in find device, to construct Device with cl::Device
            Device(cl::Device cl_device) {
                _device = cl_device;
            }

            /**
             * @brief create buffer depending on the buffertype
             *
             * @param name is the argument name
             * @param data_ptr is the pointer to the data in buffer
             * @param size in bytes is the data size in bytes
             * @param type is the BufferType: ReadOnly, WriteOnly, and ReadWrite
             * @param memory_channel_name should be a int from an array in xhl::runtime::board::alveo::u280 namespace from 0-31
             * 
             * @exception fail to create buffer
             * @exception wrong type buffer
             */
            void create_buffer(
                std::string name, void *data_ptr, size_t size_in_bytes, BufferType type,
                const int memory_channel_id
            );

            /**
             * @brief use bitstream to program the device
             *
             * @param the path of bitstream/xclbin
             * 
             * @exception fail to load bitstream
             */
            void program_device(
                const std::string &xclbin_path
            );
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

    class ComputeUnit;
    // Kernel, the declaim of compute units
    class Kernel {
        public:
            const struct KernelSignature ks;
        virtual void run() = 0;

        /**
         * @brief set argument in clkernel
         *
         * @param argument name
         * @param the computerUnit we create
         * @param argument content in any type corresponding to the name
         * 
         * @exception Failed to set argument
         * @exception Failed to match any argument name in map
         */
        template<typename T>
        void set_arg(
            const std::string& arg_name, 
            const xhl::ComputeUnit *cu,
            const T&arg_val
        );
    };

    class ComputeUnit {
        public:
            xhl::runtime::Device *cu_device;
            xhl::Kernel *cu_kernel;
            cl::Kernel clkernel;

        ComputeUnit() =delete; // don't provide default constructor 

        /**
         * @brief constructor instantiate the xhl::kernel into the ComputeUnit
         *
         * @param Kernel class under xhl namespace
         */
        ComputeUnit(xhl::Kernel *k) { // add xhl::kernel and assign it to computeunit. 
            cu_device = nullptr;
            cu_kernel = nullptr;

            this->cu_kernel = k;
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
         * @brief launch, start to run the computeunit
         *
         * @param param pack, can send any number of param in any type
         */
        template <typename... Ts>
        void launch (Ts ... ts);
    };

};

#endif //XOCL_HOSTLIB_H