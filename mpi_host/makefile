EXECS=mpi_exe
MPICC?=/opt/openmpi/bin/mpicxx

include ../utils/opencl.mk

app := pr
# default app = pr. can be sssp, bfs and so on.

APPCONFIG = ../app_udfs/$(app)

include $(APPCONFIG)/build.mk
include $(APPCONFIG)/config.mk
include $(APPCONFIG)/apply_kernel.mk

HOST_SRCS = ../libgraph/misc/graph.cpp ../libgraph/misc/data_helper.cpp

HOST_SRCS +=  ./mpi_host.cpp
HOST_SRCS += ./mpi_graph_partition.cpp
HOST_SRCS += ./mpi_graph_dataflow.cpp
HOST_SRCS += ./mpi_network.cpp
# HOST_SRCS += ../libgraph/host_graph_partition.cpp
# HOST_SRCS += ../libgraph/host_graph_dataflow.cpp

HOST_SRCS += ../common/includes/cmdparser/cmdlineparser.cpp
HOST_SRCS += ../common/includes/logger/logger.cpp
# HOST_SRCS += ../libgraph/verification/host_graph_cmodel.cpp
# HOST_SRCS += ../libgraph/verification/host_graph_verification_apply.cpp
# HOST_SRCS += ../libgraph/verification/host_graph_verification_gs.cpp
HOST_SRCS += $(APPCONFIG)/dataPrepare.cpp

CXXFLAGS += -I ./
## CXXFLAGS += -I ../libgraph
CXXFLAGS += -I ../libgraph/misc
## CXXFLAGS += -I ../libgraph/verification
## CXXFLAGS += -I ../libgraph/scheduler
CXXFLAGS += -I ../libfpga
CXXFLAGS += -I $(APPCONFIG)
CXXFLAGS += -I ../app_udfs
CXXFLAGS += -I ../common/includes/cmdparser
CXXFLAGS += -I ../common/includes/logger

# application specific parameter
CXXFLAGS += -DSUB_PARTITION_NUM=$(SUB_PARTITION_NUM)
CXXFLAGS += -DPARTITION_SIZE=$(PARTITION_SIZE)
CXXFLAGS += -DAPPLY_REF_ARRAY_SIZE=$(APPLY_REF_ARRAY_SIZE)
CXXFLAGS += -DPROCESSOR_NUM=$(PROCESSOR_NUM)

CXXFLAGS += -I$(XILINX_XRT)/include -I$(XILINX_VIVADO)/include -Wall
CXXFLAGS += -I/$(XILINX_HLS)/Vivado_HLS/include/ -O3 -g -fmessage-length=0 -std=c++14 -Wno-deprecated-declarations

# Host linker flags
LDFLAGS := -L$(XILINX_XRT)/lib -lOpenCL -pthread
LDFLAGS += -lrt -lstdc++  -lxilinxopencl
LDFLAGS += -luuid -lxrt_coreutil

all: ${EXECS}

${EXECS}: $(HOST_SRCS)
	${MPICC} $(CXXFLAGS) $(HOST_SRCS) -o mpi_host $(LDFLAGS)
clean:
	rm -f ${EXECS}