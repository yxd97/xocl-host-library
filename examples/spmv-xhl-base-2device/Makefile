include ../common.mk

# host flags for XHL
XOCL_HOST_LIB := $(REPO_ROOT)
include $(XOCL_HOST_LIB)/xhl.mk
HOST_SRCS += $(xhl_SRCS)
HOST_CC_FLAGS += $(xhl_CXXFLAGS)
HOST_LD_FLAGS += $(xhl_LDFLAGS)

# host flags for sparse-io
include $(EXAMPLES_DIR)/sparse-io/sparse-io.mk
HOST_SRCS += $(SPARSE_IO_SRCS)
HOST_CC_FLAGS += $(SPARSE_IO_CXXFLAGS)
HOST_LD_FLAGS += $(SPARSE_IO_LDFLAGS)

# include profiling infrastructure (at examples/profiling-infra.h)
HOST_CC_FLAGS += -I$(EXAMPLES_DIR)

#===============================================================================
# Project-specific variables
#===============================================================================
KERNEL_NAME := spmv
HOST_PROG_NAME := host
FREQ := 300
HLS_DIR := build_$(TARGET)_hls
LINK_DIR := build_$(TARGET)_link

#===============================================================================
# make rules
#===============================================================================
.PHONY: all exe kernel
all: exe kernel
exe: $(HOST_PROG_NAME)
kernel: $(KERNEL_NAME).xclbin emconfig.json

#===============================================================================
# Rules to build the xclbin
#===============================================================================
ifeq ($(DEBUG_KERNEL), 1)
KERNEL_OPT := -g
else
KERNEL_OPT := -O3
endif

# make .xo
KERNEL_HLS_FLAGS += -t $(TARGET)
KERNEL_HLS_FLAGS += --platform $(PLATFORM)
KERNEL_HLS_FLAGS += -k $(KERNEL_NAME)
KERNEL_HLS_FLAGS += --kernel_frequency $(FREQ)
KERNEL_HLS_FLAGS += --save-temps --temp_dir $(HLS_DIR)

$(KERNEL_NAME).xo: $(KERNEL_NAME).cpp
	$(MAKE_XO) $(KERNEL_OPT) $(KERNEL_HLS_FLAGS) $< -o $@

# make .xclbin
KERNEL_BUILD_FLAGS += -t $(TARGET)
KERNEL_BUILD_FLAGS += --platform $(PLATFORM)
KERNEL_BUILD_FLAGS += --kernel_frequency $(FREQ)
KERNEL_BUILD_FLAGS += --save-temps --temp_dir $(LINK_DIR)
KERNEL_BUILD_FLAGS += --config $(KERNEL_NAME).link.config

$(KERNEL_NAME).xclbin: $(KERNEL_NAME).xo $(KERNEL_NAME).link.config
	$(MAKE_XCLBIN) $(KERNEL_OPT) $(KERNEL_BUILD_FLAGS) $< -o $@

# emulation configuration
emconfig.json:
	emconfigutil --platform $(PLATFORM) --od .

#===============================================================================
# Rules to build host
#===============================================================================
ifeq ($(DEBUG_HOST), 1)
HOST_OPT := -g
else
HOST_OPT := -O2
endif

$(HOST_PROG_NAME): $(HOST_PROG_NAME).cpp $(HOST_SRCS)
	$(MAKE_HOST) $(HOST_OPT) $(HOST_CC_FLAGS) $(HOST_LD_FLAGS) $^ -o $@

#===============================================================================
# Cleaning
#===============================================================================
.PHONY: clean cleanall
clean:
	$(RMDIR) $(CLEAN_ENTRIES) $(HOST_PROG_NAME)

cleanall: clean
	$(RMDIR) $(CLEANALL_ENTRIES)
	$(RMDIR) $(KERNEL_NAME).xclbin* $(KERNEL_NAME).xo* xsa.xml
	$(RMDIR) $(HLS_DIR) $(LINK_DIR) $(EMCONFIG_FILE)
