$(XCLBIN)/readEdgesCU1.$(TARGET).$(DSA).xo: $(GS_KERNEL_PATH)/scatter_gather_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k readEdgesCU1 -I'$(<D)' -o'$@' '$<'

BINARY_CONTAINER_OBJS += $(XCLBIN)/readEdgesCU1.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  readEdgesCU1:2

BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_1.edgesHeadArray:DDR[0]
BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_1.vertexPushinProp:DDR[0]
BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_1.tmpVertexProp:DDR[0]
ifeq ($(strip $(HAVE_EDGE_PROP)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_1.edgeProp:DDR[0]
endif
BINARY_LINK_OBJS    += --connectivity.slr readEdgesCU1_1:SLR0

BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_2.edgesHeadArray:DDR[1]
BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_2.vertexPushinProp:DDR[1]
BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_2.tmpVertexProp:DDR[1]
ifeq ($(strip $(HAVE_EDGE_PROP)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  readEdgesCU1_2.edgeProp:DDR[1]
endif
BINARY_LINK_OBJS    += --connectivity.slr readEdgesCU1_2:SLR1

