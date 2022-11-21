ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/streamForward.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/forward_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k streamForward -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/streamForward.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  streamForward:2
BINARY_LINK_OBJS    += --connectivity.slr streamForward_1:SLR0

BINARY_LINK_OBJS    += --connectivity.slr streamForward_2:SLR0

endif
