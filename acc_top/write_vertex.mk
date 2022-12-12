ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/writeVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/write_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k writeVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/writeVertex.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  writeVertex:2
BINARY_LINK_OBJS    += --connectivity.sp  writeVertex_1.output_to_mem:DDR[0]
BINARY_LINK_OBJS    += --connectivity.slr writeVertex_1:SLR0

BINARY_LINK_OBJS    += --connectivity.sp  writeVertex_2.output_to_mem:DDR[1]
BINARY_LINK_OBJS    += --connectivity.slr writeVertex_2:SLR0

# BINARY_LINK_OBJS    += --connectivity.sp  writeVertex_3.output_to_mem:DDR[3]
# BINARY_LINK_OBJS    += --connectivity.slr writeVertex_3:SLR0

endif
