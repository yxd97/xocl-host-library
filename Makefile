ECHO = @echo

VPP = v++
CXX = g++
HLS = vitis_hls

include ./opencl/opencl.mk
include ./xcl2/xcl2.mk
CXXFLAGS += $(xcl2_CXXFLAGS)
LDFLAGS += $(xcl2_LDFLAGS)
HOST_SRCS += $(xcl2_SRCS)
CXXFLAGS += $(opencl_CXXFLAGS)
LDFLAGS += $(opencl_LDFLAGS)
CXXFLAGS += $(IDIR) -Wall -O0 -g -std=c++1y
CXXFLAGS += -fPIC

CXXFLAGS += -fmessage-length=0
LDFLAGS += -lrt -lstdc++

build: libxocl-host.so

libxocl-host.so: xocl-host-lib.o
	$(CXX) -shared -o libxocl-host.so xocl-host-lib.o

xocl-host-lib.o: xocl-host-lib.cpp xocl-host-lib.hpp
	$(CXX) $(CXXFLAGS) -c xocl-host-lib.cpp -o xocl-host-lib.o

clean:
	rm -f *.o *.so
