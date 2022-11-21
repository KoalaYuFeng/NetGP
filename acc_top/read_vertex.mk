ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/readVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/read_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k readVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/readVertex.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  readVertex:2
BINARY_LINK_OBJS    += --connectivity.sp  readVertex_1.input_from_mem:DDR[1]
BINARY_LINK_OBJS    += --connectivity.slr readVertex_1:SLR0

BINARY_LINK_OBJS    += --connectivity.sp  readVertex_2.input_from_mem:DDR[2]
BINARY_LINK_OBJS    += --connectivity.slr readVertex_2:SLR0

# BINARY_LINK_OBJS    += --connectivity.sp  readVertex_3.input_from_mem:DDR[3]
# BINARY_LINK_OBJS    += --connectivity.slr readVertex_3:SLR0

endif
