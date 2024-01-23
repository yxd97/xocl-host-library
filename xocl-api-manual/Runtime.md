## Runtime Setup APIs

### Enumerate Xilinx Devices
```cpp
// xcl::get_xil_devices() returns a list of Xilinx devices
std::vector<cl::Device> devices = xcl::get_xil_devices();
// find the device we want to program, here U280 is used as an example
// board name is the string passed to Vitis when building the bitstream, used for loading emulation model
const std::string u280_board_name = "xilinx_u280_gen3x16_xdma_1_202211_1";
// Xilinx Shell Archive (XSA) is the identifier of the real board, used for running actual bitstream
const std::string u280_board_xsa = "xilinx_u280_gen3x16_xdma_base_1";
// xcl::is_emulation() returns true if we are running in software or hardware emulation
std::string target_name = xcl::is_emulation() ? u280_board_name : u280_board_xsa;
bool found_device = false;
int dev_id = 0;
for (size_t i = 0; i < devices.size(); i++) {
    if (devices[i].getInfo<CL_DEVICE_NAME>() == target_name) {
        this->_device = devices[i];
        found_device = true;
        dev_id = i;
        break;
    }
}
if (!found_device) {
    std::cout << "[ERROR]: Failed to find " << target_name << ", exit!" << std::endl;
    exit(-1);
}
```

### Program FPGA
```cpp
// First, we need to create an OpenCL context for the device we want to program.
cl::Context cxt = cl::Context(devices[dev_id], NULL, NULL, NULL);
// Then we read the bitstream file into a array of bytes.
const std::string bitstream_file_name = "path/to/bitstream.xclbin";
std::vector<unsigned char> file_buf = xcl::read_binary_file(bitstream_file_name);
// Create a program object from the raw bytes.
cl::Program::Binaries binary{{file_buf.data(), file_buf.size()}};
// Program the device with the program object.
cl_int err = 0;
cl::Program program(cxt, {devices[dev_id]}, binary, NULL, &err);
// check if the programming is successful
if (err != CL_SUCCESS) {
    std::cout << "[ERROR]: Failed to program device with xclbin file!" << std::endl;
    exit(-1);
}
```
`CL_SUCCESS` expands to 0. The error code follows the [convention of OpenCL](https://streamhpc.com/blog/2013-04-28/opencl-error-codes/).

### Create Command Queue
```cpp
// Create a command queue for the device.
// All data movements and kernel executions are issued through the command queue.
err = 0;
cl::CommandQueue cq(cxt, devices[dev_id], CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE, &err);
```
There are mutiple flags that can be passed to the command queue constructor.
The most commonly used ones `CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` and `CL_QUEUE_PROFILING_ENABLE`.

By default, the command queue will finish the previous command before running the next one.
`CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE` allows the command queue to run commands as soon as they
are issued to the queue, which is essential for running multiple compute engines in parallel.

> We can also specify dependencies between commands in a OOO queue with the OpenCL Event API,
which are not widely used and is out of the scope of this manual.