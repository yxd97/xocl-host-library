ECHO = @echo

# Points to top directory of Git repository
COMMON_REPO = ../..
ABS_COMMON_REPO = $(shell readlink -f $(COMMON_REPO))
XOCL_HOST_LIB := $(ABS_COMMON_REPO)
XCL2_LIB_DIR := $(ABS_COMMON_REPO)/xcl2

TARGET := sw_emu
FREQ := 450
DEVICE = /opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
TEMP_DIR := ./build.$(TARGET)
BUILD_DIR := ./build_output.$(TARGET)

# CXXFLAGS += -I$(CNPY_INCLUDE)
# LDFLAGS += -L$(CNPY_LIB) -lcnpy

VPP = v++
CXX = g++
HLS = vitis_hls

#Include host Libraries
include $(ABS_COMMON_REPO)/opencl/opencl.mk
CXXFLAGS += $(opencl_CXXFLAGS)
LDFLAGS += $(opencl_LDFLAGS)

include $(ABS_COMMON_REPO)/xcl2/xcl2.mk
CXXFLAGS += $(xcl2_CXXFLAGS)
LDFLAGS += $(xcl2_LDFLAGS)
HOST_SRCS += $(xcl2_SRCS)

include $(XOCL_HOST_LIB)/xhl.mk
CXXFLAGS += $(xhl_CXXFLAGS)
LDFLAGS += $(xhl_LDFLAGS)
HOST_SRCS += $(xhl_SRCS)

# host code
HOST_SRCS += $(ABS_COMMON_REPO)/examples/burst_engine/host.cpp

# Host compiler global settings
CXXFLAGS += $(IDIR) -Wall -O0 -g
CXXFLAGS += -fmessage-length=0
CXXFLAGS += -std=c++17
LDFLAGS += -lrt -lstdc++

# kernel compiler global settings
ifeq ($(TARGET), hw)
	CLFLAGS += --kernel_frequency $(FREQ)
	LDCLFLAGS += --optimize 3
else
	CLFLAGS += -g
endif

CLFLAGS += -t $(TARGET) --platform $(DEVICE) --save-temps
CLFLAGS += $(IDIR)
# LDCLFLAGS += --config $(ABS_COMMON_REPO)/$(CFGFILE)
LDCLFLAGS +=
EXECUTABLE = host
EMCONFIG_DIR = $(TEMP_DIR)

# BINARY_CONTAINERS += $(BUILD_DIR)/burst_engine.xclbin
BINARY_CONTAINERS += ./burst_engine.xclbin
BINARY_CONTAINER_kernel_OBJS += $(TEMP_DIR)/burst_engine.xo

CP = cp -rf
RMDIR = rm -rf

.PHONY: all clean cleanall emconfig
all: $(EXECUTABLE) $(BINARY_CONTAINERS) emconfig

.PHONY: exe
exe: $(EXECUTABLE)

.PHONY: build
build: check-xrt $(BINARY_CONTAINERS)

.PHONY: hls
hls: $(TEMP_DIR)/burst_kernel.xo

#Checks for XILINX_XRT
check-xrt:
ifndef XILINX_XRT
	$(error XILINX_XRT variable is not set, please set correctly and rerun)
endif
ifndef XILINX_VITIS
	$(error XILINX_VITIS variable is not set, please set correctly and rerun)
endif

# Building kernel
$(TEMP_DIR)/burst_engine.xo: burst_engine.cpp
	mkdir -p $(TEMP_DIR)
	$(VPP) $(CLFLAGS) --temp_dir $(TEMP_DIR) -c -k burst_engine -I'$(<D)'  -o'$@' burst_engine.cpp

$(BINARY_CONTAINERS): $(BINARY_CONTAINER_kernel_OBJS)
	$(VPP) $(CLFLAGS) --temp_dir $(TEMP_DIR) -l $(LDCLFLAGS) -o'$@' $(+)

# Building Host
$(EXECUTABLE): check-xrt $(HOST_SRCS) $(HOST_HDRS)
	$(CXX) $(CXXFLAGS) $(HOST_SRCS) $(HOST_HDRS) -o '$@' $(LDFLAGS)

emconfig:$(EMCONFIG_DIR)/emconfig.json
$(EMCONFIG_DIR)/emconfig.json:
	emconfigutil --platform $(DEVICE) --od $(EMCONFIG_DIR)

check: all
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	$(CP) $(EMCONFIG_DIR)/emconfig.json .
	XCL_EMULATION_MODE=$(TARGET) ./$(EXECUTABLE) $(BINARY_CONTAINERS)
else
	./$(EXECUTABLE) $(BUILD_DIR)/burst_engine.xclbin
endif

# Cleaning stuff
clean:
	-$(RMDIR) $(EXECUTABLE) $(XCLBIN)/{*sw_emu*,*hw_emu*}
	-$(RMDIR) profile_* TempConfig system_estimate.xtxt *.rpt *.csv
	-$(RMDIR) src/*.ll *v++* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb
	-$(RMDIR) .run .Xil .ipcache

cleanall: clean
	-$(RMDIR) build*
	-$(RMDIR) package.*
	-$(RMDIR) *xclbin.run_summary qemu-memory-_* emulation/ _vimage/ pl* start_simulation.sh *.xclbin



