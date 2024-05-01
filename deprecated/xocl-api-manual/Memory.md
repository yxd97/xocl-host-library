## Memory APIs

### OpenCL Buffer
OpenCL buffer is used to model chunks of memory that can be either on the host or on the device.
You can imagine it is a pointer to host memory or device memory.
However, you cannot directly access data via a buffer.

An OpenCL buffer is created by `cl::Buffer` class.
You may think that the only information it needs to create a buffer is the
size and the data pointer, but when programming with FGPAs,
we also need to assign it to a device memory channel/bank.
This is because not every compute engine can access every memory channel/bank on
the FPGA.

Here is an example of creating a buffer on HBM channel 0:
```cpp
// assume we already created a context in variable `ctx`
// assume the actual data is in an std::vector named `data`
cl_int err = 0;
cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_EXT_PTR_XILINX;
cl_mem_ext_ptr_t ext;
ext.flags = 0 | XCL_MEM_TOPOLOGY; // assign it to HBM channel 0
ext.obj = data.data(); // associate it with the data pointer
ext.param = 0;
cl::Buffer buf(ctx, flags, data.size() * sizeof(data[0]), &ext, &err);
// error checking omitted for clarity
```

The `cl_mem_flags` is mainly used to control the access mode of the buffer.
Here access mode is respect to the device (i.e., READONLY means the device cannot write to it).
The host always have full access to the buffer.
Available flags are `CL_MEM_READ_WRITE`, `CL_MEM_WRITE_ONLY`, and `CL_MEM_READ_ONLY`,
The other two fields of the flag (`CL_MEM_USE_HOST_PTR | CL_MEM_EXT_PTR_XILINX`)
are always used together when creating a buffer on a specific memory bank for Xilinx FPGAs.

The `cl_mem_ext_ptr_t` is a struct used to associate the buffer
with a specific memory bank on the device.
The `flags` field is used to specify the memory bank, in the format of `<bank id> | XCL_MEM_TOPOLOGY`.
For U280, the available memory banks are HBM channels 0 ~ 31, with bank id 0 ~ 31;
and DDR channels 0 ~ 1, with bank id 32 ~ 33.

> Some tutorials may not use the `CL_MEM_EXT_PTR_XILINX` flag and `cl_mem_ext_ptr_t` to
associate a buffer with a memory bank on device.

> There might be other ways to create a buffer on a specific memory bank.

### Data Movement
Although you cannot access data via an OpenCL buffer, there are APIs to
sync the content of the buffer from host to device and vice versa.
The command queue provides `enqueueMigrateMemObjects()` method to do this:
```cpp
// assume we already created a command queue in variable `cq`
cl_migration_flags direction = 0;
cl_int err = cq.enqueueMigrateMemObjects({buf1, buf2}, direction)
```
The first argument is a list of buffers to be migrated.
The second argument is a flag to specify the direction of data movement.
The `cl_migration_flags` can take 0 or 1 to specify the direction of data movement.
0 means host to device, and 1 means device to host.

> A fun thing is, the OpenCL library defines a macro, `CL_MIGRATE_MEM_OBJECT_HOST`,
that expands to 1, but no macro for 0. This is because the `enqueueMigrateMemObjects()` method
belongs to a specific command queue, and the command queue is created on a specific device.

`enqueueMigrateMemObjects()` is a non-blocking. It returns immediately after the migration is launched.
A call to `finish()` of the command queue will block until all tasks in the queue are finished.

### Memory Alignment
Since the data movement between host and device is performed by PCI-e DMA, a good
memory alignment can improve the performance. If a data transfer crosses a 4KB boundary,
the DMA engine will split it into two transfers. Therefore, it is recommended to align
the data to 4KB boundary.

XOCL provides a `aligned_allocator` class which you can use when creating `std::vector`
objects to ensure the alignment:
```cpp
std::vector<int, aligned_allocator<int>> data;
```

For redable code, you can define a type alias:
```cpp
template <typename T>
using aligned_vector = std::vector<T, aligned_allocator<T>>;
aligned_vector<int> data;
```