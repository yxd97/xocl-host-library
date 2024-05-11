echo setting up Vitis-2022.1 environment......
unset LM_LICENSE_FILE
export XILINXD_LICENSE_FILE=2100@flex.ece.cornell.edu
source scl_source enable devtoolset-8
source /opt/xilinx/2022.1/Vitis/2022.1/settings64.sh > /dev/null
source /opt/xilinx/xrt/setup.sh > /dev/null
export HLS_INCLUDE=/opt/xilinx/2022.1/Vitis_HLS/2022.1/include
export LD_LIBRARY_PATH=/opt/xilinx/2022.1/Vitis/2022.1/lib/lnx64.o:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/xilinx/2022.1/Vitis/2022.1/lib/lnx64.o/Default:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$(readlink -f ./examples/sparse-io):$LD_LIBRARY_PATH
echo Vitis-2022.1 setup finished
