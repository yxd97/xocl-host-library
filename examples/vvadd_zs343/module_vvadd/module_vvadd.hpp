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

namespace xhl{
class Module_vvadd : public xhl::Module {
    public:
    std::unordered_map<std::string, ComputeUnit> _compute_units;

    void bind_cu(
        const std::unordered_map<std::string, ComputeUnit> cus
    ) override {
        this->_compute_units.insert(cus.begin(), cus.end());    
    }
    bool host(std::string xcl_bin_path);
};
} // namespace xhl
#endif // MODULE_VVADD_HPP