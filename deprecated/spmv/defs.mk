# command aliases
ECHO = @echo
CP = cp -rf
RMDIR = rm -rf
VPP_HLS = v++ -c
VPP_BUILD = v++ -l
CC = g++
VIVADO = vivado -mode batch -nojournal -notrace -source

# directories
HLS_BUILD_DIR := $(PROJECT_NAME)/build.$(KERNEL_NAME).compile.$(TARGET)
VIVADO_BUILD_DIR := $(PROJECT_NAME)/build.$(KERNEL_NAME).link.$(TARGET)

# host flags
XOCL_HOST_LIB = $(EXAMPLES_DIR)/..
include $(XOCL_HOST_LIB)/xcl2/xcl2.mk
HOST_SRCS += $(xcl2_SRCS)
HOST_CC_FLAGS += $(xcl2_CXXFLAGS)
HOST_LD_FLAGS += $(xcl2_LDFLAGS)
include $(XOCL_HOST_LIB)/opencl/opencl.mk
HOST_SRCS += $(opencl_SRCS)
HOST_CC_FLAGS += $(opencl_CXXFLAGS)
HOST_LD_FLAGS += $(opencl_LDFLAGS)
HOST_CC_FLAGS += -I$(XOCL_HOST_LIB)

# kernel flags
KERNEL_GLOBAL_FLAGS += -t $(TARGET) --platform $(DEVICE) --save-temps
KERNEL_GLOBAL_FLAGS += --kernel_frequency $(FREQ)
ifeq ($(TARGET), hw)
	KERNEL_GLOBAL_FLAGS += --optimize 3
	ifeq ($(PROFILE), 1)
		KERNEL_GLOBAL_FLAGS += -g
	endif
else
	KERNEL_GLOBAL_FLAGS += -g
endif
KERNEL_HLS_FLAGS += $(KERNEL_GLOBAL_FLAGS)
KERNEL_HLS_FLAGS += --temp_dir $(HLS_BUILD_DIR) -k $(KERNEL_NAME)
KERNEL_BUILD_FLAGS += $(KERNEL_GLOBAL_FLAGS)
KERNEL_BUILD_FLAGS += --temp_dir $(VIVADO_BUILD_DIR)

# check if building for emulation
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	EMULATION := 1
endif
