## Kernel APIs

## Create Kernel Object(s)
There are two distinct ways to create a kernel object.
One is for there is only one instance of a HLS kernel,
and there is no `nk` tags applied during Vitis linking.


```cpp
const std::string kernel_name = "hls_top_function_name";
err = 0;
cl::Kernel knl(program, kernel_name.c_str(), &err);
// error checking is omitted here
```

The other is for there are multiple instances of a HLS kernel,
and `nk` tags are applied during Vitis linking.
> For semantics of `nk` tags, please refer to [Vitis Command Reference](https://docs.xilinx.com/r/en-US/ug1393-vitis-application-acceleration/connectivity-Options).
```cpp
// assume the nk tag is applied as:
// nk=hls_top_function_name:4:k0,k1,k2,k3
const std::string kernel_name = "hls_top_function_name";
std::vector<cl::Kernel> knls(4);
for (int i = 0; i < 4; i++) {
    const std::string instance_name = "k" + std::to_string(i);
    std::string instance_path = kernel_name + ":{" + instance_name + "}";
    err = 0;
    knl[i] = cl::Kernel(program, instance_path.c_str(), &err);
    // error checking is omitted here
}
```

## Pass Arguments to Kernels
XOCL kernels all have an addressable register file to hold argument values.
Host writes to this register file to pass arguments to the kernel.
All arguments passed from the host are passed by value.

> HLS kernels never write to the register file.

> RTL kernels may update the register file,
but there are no APIs to read back the updated values in XOCL (XRT APIs can read it).

> `hls::stream` arguments appear as passed-by-reference, but they are not handled on the host side.

The kernel arguments are set through the `setArg()` method of the kernel object:
```cpp
/* HLS top function:
void add_double_to_int_vector (
    const int* vector,
    double* result,
    const unsigned size,
    const double value
);
*/
int argc = 0;
err = knl.setArg(argc++, OpenCLBuffer_for_input);
err = knl.setArg(argc++, OpenCLBuffer_for_result);
err = knl.setArg(argc++, 1024U);
err = knl.setArg(argc++, 3.1415926);
// error checking is omitted here
```

> Please refer to [Memory APIs](Memory.md) for details on OpenCL buffers.

The `setArg()` method does NOT do any data conversion.
If the last line of the example is `err = knl.setArg(argc++, 3.1415926f);`,
the error code will not be `CL_SUCCESS` due to the size mismatch between `float` and `double`.

There is one exception. When the kernel contains `hls::stream` arguments,
such arguments are not controlled by the `setArg()` method.
However, they still count towards the argument index.

> These `hls::stream` arguments are handled during Vitis linking, via the `sc` tags. See [Vitis Command Reference](https://docs.xilinx.com/r/en-US/ug1393-vitis-application-acceleration/connectivity-Options) for details.

```cpp
/* HLS top function:
void add_double_to_int_vector (
    const int* vector,
    hls::stream<double> &result, <-- appear as passed-by-reference
    const unsigned size,
    const double value
);
*/
int argc = 0;
err = knl.setArg(argc++, OpenCLBuffer_for_input);
argc++; // skip the hls::stream argument
err = knl.setArg(argc++, 1024U);
err = knl.setArg(argc++, 3.1415926);
// error checking is omitted here
```

## Launch Kernel(s)