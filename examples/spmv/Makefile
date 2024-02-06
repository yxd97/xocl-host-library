# get the absolute path of the current directory
EXAMPLES_DIR = $(shell readlink -f .)

#===============================================================================
# help message
#===============================================================================

#===============================================================================
# project settings
#===============================================================================
DATASET :=
KERNEL_NAME := spmv
PROJECT_NAME := ScSpMV
TRACE := 0

FREQ := 450
DEVICE = /opt/xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
TARGET := hw_emu

include $(EXAMPLES_DIR)/defs.mk

#===============================================================================
#  Host files
#===============================================================================
HOST_SRCS += $(EXAMPLES_DIR)/$(PROJECT_NAME)/host.cpp
HOST_HDRS += $(EXAMPLES_DIR)/$(PROJECT_NAME)/sp_data_structure.h
HOST_CC_FLAGS += -g
HOST_LD_FLAGS += # link paths, etc

#===============================================================================
#  Kernel files
#===============================================================================
KERNEL_SRCS += $(EXAMPLES_DIR)/$(PROJECT_NAME)/$(KERNEL_NAME).cpp
# uncomment the following lines to set configurations to HLS (such as enable profiling)
# KERNEL_HLS_CONFIG_FILE := $(EXAMPLES_DIR)/$(PROJECT_NAME)/compile_config.cfg
# KERNEL_HLS_FLAGS += --config $(KERNEL_HLS_CONFIG_FILE)
KERNEL_BUILD_CONFIG_FILE := $(EXAMPLES_DIR)/$(PROJECT_NAME)/link_config.cfg
KERNEL_BUILD_FLAGS += --config $(KERNEL_BUILD_CONFIG_FILE)

include $(EXAMPLES_DIR)/rules.mk
