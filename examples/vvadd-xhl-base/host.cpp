#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    // usage: host <xclbin_path> <vector size>
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << "<exec mode> <xclbin_path> <vector size>" << std::endl;
        return 1;
    }

    // TODO: write host with XHL base API (using Device, KernelSignature, ComputeUnit, ...)

    return 0;
}
