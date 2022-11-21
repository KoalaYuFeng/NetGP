ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/streamForward.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/forward_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k streamForward -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/streamForward.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  streamForward:%THUNDERGP_KERNEL_NUM%
BINARY_LINK_OBJS    += --connectivity.slr streamForward_%THUNDERGP_KERNEL_NAME%:SLR%THUNDERGP_FORWARD_KERNEL_SLR%

endif
