SHELL          := /bin/bash
TARGET         := $(TARGETS)
DEVICE         := $(DEVICES)

COMMON_REPO     = ./
ABS_COMMON_REPO = $(shell readlink -f $(COMMON_REPO))
UTILS_PATH      = ./utils

.PHONY: all clean exe hwemuprepare ${EXECS} emconfig 
all: precheck
exe: precheck
clean: precheck
hwemuprepare: precheck


precheck: 
ifndef app
	$(error app is undefined)
else
APP = $(app)
APPCONFIG = ./app_udfs/$(APP)

include $(UTILS_PATH)/help.mk
include $(UTILS_PATH)/utils.mk

# include acc_para/para.mk

include $(APPCONFIG)/config.mk
include $(APPCONFIG)/build.mk
include ./app_udfs/common.mk

# $(shell ./utils/automation.sh $(app) $(KERNEL_NUM) $(HAVE_APPLY) > /dev/null )

ifeq ($(NODE_TYPE), GAS)
	include acc_top/NetGP_GAS.mk
else ifeq ($(NODE_TYPE), GS)
	include acc_top/NetGP_GS.mk
else
	@echo "NODE TYPE not match"
	exit 1
endif


include $(UTILS_PATH)/bitstream.mk
include $(UTILS_PATH)/clean.mk
include ./mpi_host/mpi_host.mk


all: $(BINARY_CONTAINERS) emconfig ${EXECS}
exe: ${EXECS}

endif
