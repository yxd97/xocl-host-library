# Frequently Used APIs of Xilinx OpenCL

## Introduction
Xilinx OpenCL (XOCL) is a runtime library developed by Xilinx (now AMD).
It provides a set of OpenCL-compatiable APIs for programming with AMD FPGAs.

Compared to Xilinx Runtime (XRT), XOCL runs at a higher level of abstruction.
Programmers using XOCL no longer need to think about register mappings and
control protocols.

This manual aims to provide a quick reference for the most basic and frequently used APIs.

## General Steps to Run a Design on FPGA with XOCL
1. Setup the runtime, including finding the correct device, program the device, and creating OpenCL runtime objects
such as contexts and command queues.
2. Prepare input data and create OpenCL buffers for them.
3. Create output buffers and their OpenCL buffers as well.
4. Migrate the input data to the device through the command queue.
5. Set arguments for compute engines.
6. Wait for the data migration to finish.
7. Launch the compute engines through the command queue.
8. Wait for the compute engines to finish.
9. Migrate the output data back to the host through the command queue.
10. Wait for the data migration to finish.

## APIs
[Runtime Setup APIs](Runtime.md)

[Kernel APIs](Kernel.md)

[Memory APIs](Memory.md)

## Problems/Limitations of XOCL
 * The contexts, command queues, and memory buffers are too low-level for application developers.
 * One `cl::Kernel` must correspond to one compute engine on the hardware,
 which does not support overlay architectures and split-kernel designs well.
 * The `cl::Buffer` only handles data movement and argument passing, you cannot access data through it.
 * Exposes every detail of multi-FPGA programming.
