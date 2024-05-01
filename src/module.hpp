#ifndef MODULE_HPP
#define MODULE_HPP

#include <vector>
#include <unordered_map>
#include <iostream>
#include <string>

#include "xcl2.hpp"
#include "compute_unit.hpp"

namespace xhl {
class Module {
    private:
    // std::string xcl_bin_path;
    // std::unordered_map<std::string, ComputeUnit> _compute_units;
    
    public:
    virtual void initialize(std::vector<Device>& devices) = 0;
    // Module(std::vector<Device>& devices) {
    //     this->initialize(devices);
    // }

    virtual void free_cu() = 0;
    // ~Module(){
    //     this->free_cu();
    // }

    // virtual void bind_cu(const std::unordered_map<std::string, ComputeUnit> cus) = 0;
};
} // namespace xhl


#endif // MODULE_HPP