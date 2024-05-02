#ifndef MODULE_VVADD_HPP
#define MODULE_VVADD_HPP

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <string>

#include "xocl-host-lib.hpp"
#include "compute_unit.hpp"
#include "module.hpp"
#include "device.hpp"

namespace xhl{
class Module_vvadd : public xhl::Module {
    public:
    ComputeUnit* vvadd_cu;

    const KernelSignature vvadd = {
        "vvadd", {
            {"in1", "int*"},
            {"in2", "int*"},
            {"out", "int*"},
            {"DATA_SIZE", "int"}
        }
    };

    void initialize(std::vector<Device>& devices) override;

    void free_cu() override;

    ~Module_vvadd();

    std::vector<int> run(
        std::vector<int> in1, 
        std::vector<int> in2,
        std::vector<int> out
    );
};
} // namespace xhl
#endif // MODULE_VVADD_HPP