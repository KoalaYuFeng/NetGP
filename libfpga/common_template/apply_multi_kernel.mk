ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/vertexApply.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/apply_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k vertexApply -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/vertexApply.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  vertexApply:%THUNDERGP_KERNEL_NUM%
BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_%THUNDERGP_KERNEL_NAME%.vertexProp:DDR[%THUNDERGP_APPLY_PORT_ID%]

ifeq ($(strip $(HAVE_APPLY_OUTDEG)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_%THUNDERGP_KERNEL_NAME%.outDegree:DDR[%THUNDERGP_APPLY_PORT_ID%]
endif

BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_%THUNDERGP_KERNEL_NAME%.outReg:DDR[%THUNDERGP_APPLY_PORT_ID%]
BINARY_LINK_OBJS    += --connectivity.slr vertexApply_%THUNDERGP_KERNEL_NAME%:SLR%THUNDERGP_APPLY_KERNEL_SLR%

BINARY_LINK_OBJS    += --config ./acc_top/apply.cfg

endif
