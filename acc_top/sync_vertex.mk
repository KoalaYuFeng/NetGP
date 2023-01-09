ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/syncVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/sync_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k syncVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/syncVertex.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  syncVertex:1
BINARY_LINK_OBJS    += --connectivity.slr syncVertex_1:SLR0

# BINARY_LINK_OBJS    += --connectivity.sp  readVertex_3.input_from_mem:DDR[3]
# BINARY_LINK_OBJS    += --connectivity.slr readVertex_3:SLR0

endif