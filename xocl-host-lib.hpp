#include "xcl2/xcl2.hpp"
#include <vector>

// config words
enum MigrateDirection {ToDevice, ToHost};
enum BufferType {ReadOnly, WriteOnly, ReadWrite};
enum ExecMode {SW_EMU, HW_EMU, HW};

// alias for argument map
using ArgumentMap = std::unordered_map<std::string, string>;

