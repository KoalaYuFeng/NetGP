## apply kernel 
ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/vertexApply.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/apply_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k vertexApply -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/vertexApply.$(TARGET).$(DSA).xo

ifeq ($(strip $(HAVE_APPLY_OUTDEG)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_1.outDegree:DDR[0]
endif

endif

## forward kernel 
$(XCLBIN)/streamForward.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/forward_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k streamForward -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/streamForward.$(TARGET).$(DSA).xo

## merge kernel 
$(XCLBIN)/streamMerge.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/merge_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k streamMerge -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/streamMerge.$(TARGET).$(DSA).xo

## sync kernel 
$(XCLBIN)/syncVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/sync_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k syncVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/syncVertex.$(TARGET).$(DSA).xo

## gather-scatter kernel 
$(XCLBIN)/gatherScatter.$(TARGET).$(DSA).xo: $(GS_KERNEL_PATH)/scatter_gather_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k gatherScatter -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/gatherScatter.$(TARGET).$(DSA).xo

ifeq ($(strip $(HAVE_EDGE_PROP)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  gatherScatter_1.edgeProp:DDR[0]
endif

ifeq ($(strip $(HAVE_EDGE_PROP)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  gatherScatter_2.edgeProp:DDR[2]
endif

ifeq ($(strip $(HAVE_EDGE_PROP)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  gatherScatter_3.edgeProp:DDR[3]
endif

# write kernel
$(XCLBIN)/writeVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/write_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k writeVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/writeVertex.$(TARGET).$(DSA).xo
