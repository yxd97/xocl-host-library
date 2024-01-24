# Frequently Used APIs of Xilinx OpenCL

## Introduction
Xilinx OpenCL (XOCL) is a runtime library developed by Xilinx (now AMD).
It provides a set of OpenCL-compatiable APIs for programming with AMD FPGAs.

Compared to Xilinx Runtime (XRT), XOCL runs at a higher level of abstruction.
Programmers using XOCL no longer need to think about register mappings and
control protocols.

This manual aims to provide a quick reference for the most basic and frequently used APIs.

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
