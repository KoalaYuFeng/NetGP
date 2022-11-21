ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/readVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/read_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k readVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/readVertex.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  readVertex:%THUNDERGP_KERNEL_NUM%
BINARY_LINK_OBJS    += --connectivity.sp  readVertex_%THUNDERGP_KERNEL_NAME%.input_from_mem:DDR[%THUNDERGP_READ_PORT_ID%]
BINARY_LINK_OBJS    += --connectivity.slr readVertex_%THUNDERGP_KERNEL_NAME%:SLR%THUNDERGP_READ_KERNEL_SLR%
endif