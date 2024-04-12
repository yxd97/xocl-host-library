#include "xocl-host-lib.hpp"

#include "xocl-host-lib/compute_unit.hpp"

/* in compute_unit.hpp
 class ComputeUnit {

public:
	void run()


};


*/


class VaddCU : ComputeUnit {
public:
	void run(std::vector<int> &a, ...) {
		...
		...
	}

};


int main(int argc, char** argv) {
	std::string xclbin = argv[1];
	std::vector<Device> devices = runtime::find_devices(
		device::alveo::u280::identifier // device::DeviceIdentifier -- const struct
	); // raise std::runtime_error if no devices are available

	runtime::program_device(
		devices[0],
		xclbin
	); // raise

	// "vvadd"
	/*
 		void vvadd (
			const int* a,
			const int* b,
			int* c,
			const unsigned size
		);
 	*/

	kernel::KernelSignature kernel_vvadd(
		{
			{"const int*", "a"},
			{"const int*", "b"},
			{"int*", "c"},
			{"const unsigned", "size"}
		} // std::map<std::string, std::string>
	);

	std::map<std::string, compute_unit::ComputeUnit> cus = kernel::instantiate(
		kernel_vvadd,
		device[0],
		{"vadd_A", "vadd_B", "vadd_C"}, // std::vector<std::string>
		// 3 // size_t
    ); // raise ...

	std::vector<int> a[3];
	std::vector<int> b[3];
	std::vector<int> c[3];

	// initialization omitted

	// data migration
	device[0].move_data({a[0], a[1], a[2], b[0], b[1], b[2]}, ToDevice /*Enum*/ ); // enqueueMigrateMemoryObjects + finish

	cus["vadd_A"](a[0], b[0], c[0], non_blocking = true);
	cus["vadd_B"](a[1], b[1], c[1]); // == non_blocking + device.finish();
}