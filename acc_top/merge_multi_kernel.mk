ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/streamMerge.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/merge_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k streamMerge -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/streamMerge.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  streamMerge:2
BINARY_LINK_OBJS    += --connectivity.slr streamMerge_1:SLR0

BINARY_LINK_OBJS    += --connectivity.slr streamMerge_2:SLR0

endif
