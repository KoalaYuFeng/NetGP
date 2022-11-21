ifeq ($(strip $(HAVE_APPLY)), $(strip $(VAR_TRUE)))
$(XCLBIN)/vertexApply.$(TARGET).$(DSA).xo: $(APPLY_KERNEL_PATH)/apply_multi_top.cpp
	mkdir -p $(XCLBIN)
	$(XOCC) $(CLFLAGS) -c -k vertexApply -I'$(<D)' -o'$@' '$<'
BINARY_CONTAINER_OBJS += $(XCLBIN)/vertexApply.$(TARGET).$(DSA).xo

BINARY_LINK_OBJS    += --connectivity.nk  vertexApply:1
BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_1.vertexProp:DDR[0]

ifeq ($(strip $(HAVE_APPLY_OUTDEG)), $(strip $(VAR_TRUE)))
BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_1.outDegree:DDR[0]
endif

BINARY_LINK_OBJS    += --connectivity.sp  vertexApply_1.outReg:DDR[0]
BINARY_LINK_OBJS    += --connectivity.slr vertexApply_1:SLR0

## BINARY_LINK_OBJS    += --config ./acc_top/apply.cfg

endif

