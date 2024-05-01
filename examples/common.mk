# command aliases
ECHO = @echo
CP = cp -rf
RMDIR = rm -rf
CC = g++

MAKE_XO = v++ -c
MAKE_XCLBIN = v++ -l
MAKE_HOST = $(CC)

# the absolute path to the examples directory
REPO_ROOT := $(shell readlink -f ../..)
EXAMPLES_DIR := $(REPO_ROOT)/examples

# host flags for OpenCL and XRT
XCL2_LIB_DIR := $(REPO_ROOT)/xcl2
include $(REPO_ROOT)/xcl2/xcl2.mk
HOST_SRCS += $(xcl2_SRCS)
HOST_CC_FLAGS += $(xcl2_CXXFLAGS)
HOST_LD_FLAGS += $(xcl2_LDFLAGS)
include $(REPO_ROOT)/opencl/opencl.mk
HOST_SRCS += $(opencl_SRCS)
HOST_CC_FLAGS += $(opencl_CXXFLAGS)
HOST_LD_FLAGS += $(opencl_LDFLAGS)

# build targets: sw_emu, hw_emu, hw
TARGET := sw_emu
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	EMULATION := 1
endif

# target hardware platform
PLATFORM = /opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm

# help text
.PHONY: help
help:
	$(ECHO) "Makefile usage:"
	$(ECHO) ""
	$(ECHO) "  make/make help: print this help message"
	$(ECHO) ""
	$(ECHO) "  make all: build the host and kernel"
	$(ECHO) "    TARGET=[sw_emu|hw_emu|hw]: kernel build target "
	$(ECHO) "      default: sw_emu"
	$(ECHO) "    DEBUG_HOST=[0|1]: build host with -g"
	$(ECHO) "      default: 0 (-O2)"
	$(ECHO) "    DEBUG_KERNEL=[0|1]: build kernel with -g"
	$(ECHO) "      default: 0 (-O3)"
	$(ECHO) ""
	$(ECHO) "  make exe: build the host program"
	$(ECHO) "    DEBUG_HOST=[0|1]: build host with -g"
	$(ECHO) "      default: 0 (-O2)"
	$(ECHO) ""
	$(ECHO) "  make kernel: build the kernel xclbin"
	$(ECHO) "    TARGET=[sw_emu|hw_emu|hw]: kernel build target "
	$(ECHO) "      default: sw_emu"
	$(ECHO) "    DEBUG_KERNEL=[0|1]: build kernel with -g"
	$(ECHO) "      default: 0 (-O3)"
	$(ECHO) ""
	$(ECHO) "  make clean: remove all generated files that are easy to regenerate"
	$(ECHO) ""
	$(ECHO) "  make cleanall: remove all temporary files, kernel xclbin, and build projects"\
	        "which may take hours to regenerate!"

# clean entries
CLEAN_ENTRIES += profile_* TempConfig system_estimate.xtxt *.rpt
CLEAN_ENTRIES += timeline_kernels.csv profile_kernels.csv
CLEAN_ENTRIES += device_trace*.csv summary.csv
CLEAN_ENTRIES += vitis_analyzer*.str
CLEAN_ENTRIES += src/*.ll *v++* dltmp* xmltmp* *.log *.jou
CLEAN_ENTRIES += emconfig.json ext_metadata.json
CLEAN_ENTRIES += .run *.protoinst *run_summary

CLEANALL_ENTRIES += .Xil .ipcache package.* *xclbin.run_summary
