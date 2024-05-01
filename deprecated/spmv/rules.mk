# Checks for XILINX_XRT
check-xrt:
ifndef XILINX_XRT
	$(error XILINX_XRT variable is not set, please set correctly and rerun)
endif
ifndef XILINX_VITIS
	$(error XILINX_VITIS variable is not set, please set correctly and rerun)
endif

# Enable/disable xrt.ini
.PHONY: enable_xrt.ini disable_xrt.ini
enable_xrt.ini:
	mv xrt.ini.disabled xrt.ini

disable_xrt.ini:
	mv xrt.ini xrt.ini.disabled

# run HLS on the kernel
HLS_XO := $(HLS_BUILD_DIR)/$(KERNEL_NAME).xo

.PHONY: hls
hls: $(HLS_XO)

$(HLS_XO): $(KERNEL_SRCS)
	mkdir -p $(HLS_BUILD_DIR)
	$(VPP_HLS) $(KERNEL_HLS_FLAGS) -o $@ $(KERNEL_SRCS)

# run autotrace to instrument the kernel
AT_XO := $(AT_BUILD_DIR)/export/trace_$(KERNEL_NAME).xo

.PHONY: autotrace
autotrace: $(AT_XO)

$(AT_XO): $(HLS_XO) $(TRACE_CONFIG_FILE)
	mkdir -p $(AT_BUILD_DIR)
	$(AUTOTRACE) $(TRACE_CONFIG_FILE) $(KERNEL_TOP) $(AT_ARGS)
	cd $(AT_BUILD_DIR); $(VIVADO) $(AT_PACKXO_SCRIPT) $(AT_PACKXO_ARGS)

# run Vitis to build the kernel
BITSTREAM += $(VIVADO_BUILD_DIR)/$(KERNEL_NAME).xclbin
ifeq ($(TRACE), 1)
	XO_FOR_BITSTREAM := $(AT_XO)
else
	XO_FOR_BITSTREAM := $(HLS_XO)
endif

.PHONY: build
build: check-xrt $(BITSTREAM)
$(BITSTREAM): $(XO_FOR_BITSTREAM)
	$(VPP_BUILD) $(KERNEL_BUILD_FLAGS) -o $@ $(+)

# build host binary
HOST_EXE = $(ABS_COMMON_REPO)/$(PROJECT_NAME)/$(KERNEL_NAME)_host

.PHONY: exe
exe: $(HOST_EXE)

$(HOST_EXE): $(HOST_SRCS) $(HOST_HDRS)
	$(CXX) $(HOST_CC_FLAGS) $(HOST_SRCS) -o $@ $(HOST_LD_FLAGS)

# emulation configuration
EMCONFIG_FILE = $(ABS_COMMON_REPO)/$(PROJECT_NAME)/emconfig.json

.PHONY: emconfig
emconfig: $(EMCONFIG_FILE)

$(EMCONFIG_FILE):
	emconfigutil --platform $(DEVICE) --od $(ABS_COMMON_REPO)/$(PROJECT_NAME)

# run the host binary
.PHONY: run
run: $(HOST_EXE) $(BITSTREAM) $(if $(EMULATION), emconfig)
ifeq ($(EMULATION), 1)
	XCL_EMULATION_MODE=$(TARGET) $(HOST_EXE) $(BITSTREAM)
else
	$(HOST_EXE) $(BITSTREAM)
endif

#  Cleaning stuff
.PHONY: clean cleanall

clean:
	$(RMDIR) $(HOST_EXE) $(XCLBIN)/{*sw_emu*,*hw_emu*}
	$(RMDIR) profile_* TempConfig system_estimate.xtxt *.rpt
	$(RMDIR) timeline_kernels.csv profile_kernels.csv
	$(RMDIR) device_trace*.csv summary.csv
	$(RMDIR) vitis_analyzer*.str
	$(RMDIR) src/*.ll *v++* dltmp* xmltmp* *.log *.jou
	$(RMDIR) emconfig.json ext_metadata.json
	$(RMDIR) .run *.protoinst *run_summary

cleanall: clean
	$(RMDIR) .Xil .ipcache
	$(RMDIR) $(PROJECT_NAME)/build*
	$(RMDIR) package.*
	$(RMDIR) *xclbin.run_summary
