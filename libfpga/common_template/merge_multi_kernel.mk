ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/streamMerge.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/merge_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k streamMerge -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/streamMerge.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  streamMerge:%THUNDERGP_KERNEL_NUM%
BINARY_LINK_OBJS    += --connectivity.slr streamMerge_%THUNDERGP_KERNEL_NAME%:SLR%THUNDERGP_MERGE_KERNEL_SLR%

endif
