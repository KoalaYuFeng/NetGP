ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/writeVertex.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/write_vertex.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k writeVertex -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/writeVertex.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  writeVertex:%THUNDERGP_KERNEL_NUM%
BINARY_LINK_OBJS    += --connectivity.sp  writeVertex_%THUNDERGP_KERNEL_NAME%.output_to_mem:DDR[%THUNDERGP_WRITE_PORT_ID%]
BINARY_LINK_OBJS    += --connectivity.slr writeVertex_%THUNDERGP_KERNEL_NAME%:SLR%THUNDERGP_WRITE_KERNEL_SLR%
endif