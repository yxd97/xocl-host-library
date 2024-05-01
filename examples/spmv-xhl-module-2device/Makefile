include ../common.mk

# host flags for XHL
XOCL_HOST_LIB := $(REPO_ROOT)
include $(XOCL_HOST_LIB)/xhl.mk
HOST_SRCS += $(xhl_SRCS)
HOST_CC_FLAGS += $(xhl_CXXFLAGS)
HOST_LD_FLAGS += $(xhl_LDFLAGS)

#===============================================================================
# Project-specific variables
#===============================================================================
KERNEL_NAME := # TODO: set kernel name
HOST_PROG_NAME := # TODO: set host program name
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

# TODO: add rules to make .xo

# make .xclbin
KERNEL_BUILD_FLAGS += -t $(TARGET)
KERNEL_BUILD_FLAGS += --platform $(PLATFORM)
KERNEL_BUILD_FLAGS += --kernel_frequency $(FREQ)
KERNEL_BUILD_FLAGS += --save-temps --temp_dir $(LINK_DIR)

# TODO: add rules to make .xclbin

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

# TODO: add rules to build host

#===============================================================================
# Cleaning
#===============================================================================
.PHONY: clean cleanall
clean:
	$(RMDIR) $(CLEAN_ENTRIES) $(HOST_PROG_NAME)

cleanall: clean
	$(RMDIR) $(CLEANALL_ENTRIES)
	$(RMDIR) $(KERNEL_NAME).xclbin $(KERNEL_NAME).xo
	$(RMDIR) $(HLS_DIR) $(LINK_DIR) $(EMCONFIG_FILE)
