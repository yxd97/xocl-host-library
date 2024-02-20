#include "xcl2/xcl2.hpp"
#include <vector>
#include "xocl-host-lib.hpp"
#include <unordered_map>

// config words
enum MigrateDirection {ToDevice, ToHost};
enum BufferType {ReadOnly, WriteOnly, ReadWrite};
enum ExecMode {SW_EMU, HW_EMU, HW};

// alias for argument map
using ArgumentMap = std::unordered_map<std::string, string>;

ExecMode check_execution_mode() {
    if(xcl::is_emulation()) {
        if(xcl::is_hw_emulation())
            return ExecMode::HW_EMU;
        return ExecMode::SW_EMU;
    }
    return ExecMode::HW;
}

namespace XHL {
    struct BoardIdentifier {
        std::string name;
        std::string xsa;
    };
    struct KernelSignature {
        // from argument type to argument name, string to string
        std::string name; // kernel name
        std::unordered_map<std::string, std::string> argmap; // argument map
    };

    namespace boards {
        namespace alveo {
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

    // runtime namespace, only happens on run time
    namespace runtime {
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
            Device() {
                _errflag = 0;
                // TODO
            }
            // used in find device, to construct Device with cl::Device
            Device(cl::Device cl_device) {
                _device = cl_device;
            }
            // create buffer
            void create_buffer(
                std::string name, void *data_ptr, size_t size_in_bytes, BufferType type,
                const int memory_channel_id
            ) {
                cl_mem_ext_ptr_t eptr;
                eptr.flags = memory_channel_id;
                eptr.obj = data_ptr;
                eptr.param = 0;
                this->_ext_ptrs[name] = eptr;
                switch (type) {
                    case ReadOnly:
                        this->buffers[name] = cl::Buffer(
                            this->_context,
                            CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                            size_in_bytes,
                            &(this->_ext_ptrs[name]),
                            &(this->_errflag)
                        );
                        if (this->_errflag) {
                            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                        }
                    break;
                    case WriteOnly:
                        this->buffers[name] = cl::Buffer(
                            this->_context,
                            CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                            size_in_bytes,
                            &(this->_ext_ptrs[name]),
                            &(this->_errflag)
                        );
                        if (this->_errflag) {
                            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                        }
                    break;
                    case ReadWrite:
                        this->buffers[name] = cl::Buffer(
                            this->_context,
                            CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                            size_in_bytes,
                            &(this->_ext_ptrs[name]),
                            &(this->_errflag)
                        );
                        if (this->_errflag) {
                            throw std::runtime_error("[ERROR]: Create buffer error, exit!\n");
                        }
                    break;
                    default:
                        std::cout << "[ERROR]: Unsupported buffer type!"<< std::endl;
                    break;
                }
            }
            void program_device(
                const cl::Device &device_name, 
                const std::string &xclbin_path
            ) {
                // load bitstream into FPGA
                cl::Context cxt = cl::Context(devices[dev_id], NULL, NULL, NULL);

                const std::string bitstream_file_name = xclbin_path;
                std::vector<unsigned char> file_buf = xcl::read_binary_file(bitstream_file_name);
                cl::Program::Binaries binary{{file_buf.data(), file_buf.size()}};

                cl_int err = 0;
                cl::Program program(cxt, {devices[dev_id]}, binary, NULL, &err);
                if (err != CL_SUCCESS) {
                    throw std::runtime_error("[ERROR]: Load bitstream failed, exit!\n");
                }
            }
        };
        std::vector<Device> find_devices(const device::alveo::u280::BoardIdentifier &identifier) {
            // find device, push found device into our devices
            std::vector<Device> devices;
            std::string target_name =
                (check_execution_mode() == ExecMode::HW)
                ? identifier.xsa : identifier.name;
            bool found_device = false;
            auto cl_devices = xcl::get_xil_devices();
            for (size_t i = 0; i < cl_devices.size(); i++) {
                if (cl_devices[i].getInfo<CL_DEVICE_NAME>() == target_name) {
                    Device tmp_device(cl_devices[i]); // use special constructor function
                    devices.push_back(tmp_device);
                    found_device = true;
                }
            }
            if (!found_device) {
                throw std::runtime_error("[ERROR]: Failed to find, exit!\n");
            }
            return devices;
        }
    };

    // Compute Unit, equals kernel in OpenCL, running on Devices
    class ComputeUnit {
        public:
            Device *cu_device;
            Kernel *cu_kernel;
            cl::kernel clkernel;
        ComputeUnit() {
            cu_device = nullptr;
            cu_kernel = nullptr;
        }

        void bind (Kernel *k, Device *d) {
            this->cu_kernel = k;
            this->cu_device = d;
            // create CL kernel in compute unit
            cl_int errflag = 0;
            this->clkernel = cl::Kernel(
                this->cu_device->_program, this->cu_kernel->ks.name.c_str(), &errflag
            );
            if (errflag) {
                throw std::runtime_error("[ERROR]: Failed to create CL Kernel, exit!\n");
            }
        }
        // use param pack
        template <typename... Ts>
        void launch (Ts ... ts) {
            this->cu_kernel->run(this, ts...); 
            this->cu_device->command_q.enqueueTask(this->clkernel);
        }
    };

    // Kernel, the declaim of compute units
    class Kernel {
        public:
            const struct KernelSignature ks;

        virtual void run() = 0;
        // why string to string map
        template<typename T>
        void set_arg(
            const std::string& arg_name, 
            const ComputeUnit *cu,
            const T&arg_val
        ) {
            cl_int errflag = 0;
            errflag = cu->clkernel.setArg(,arg_val); // TODO: what is the first argument?
        }
    };

};

