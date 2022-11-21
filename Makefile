include ThunderGP.mk

.PHONY: all clean exe hwemuprepare

INTERFACE := 1 ## using interface 1

NETLAYERDIR = NetLayers/
CMACDIR     = Ethernet/

NETLAYERHLS = 100G-fpga-network-stack-core

TEMP_DIR := _x
BINARY_CONTAINER_OBJS = $(NETLAYERDIR)$(TEMP_DIR)/networklayer.xo

CONFIGFLAGS = --config ./connection.ini
#CONFIGFLAGS += --kernel_frequency 280

# Include cmac kernel depending on the interface
BINARY_CONTAINER_OBJS += $(CMACDIR)$(TEMP_DIR)/cmac_1.xo

# Linker parameters
# Linker userPostSysLinkTcl param
ifeq (u5,$(findstring u5, $(DEVICES)))
	HLS_IP_FOLDER  = $(shell readlink -f ./$(NETLAYERDIR)$(NETLAYERHLS)/synthesis_results_HBM)
else ifeq (u280,$(findstring u280, $(DEVICES)))
	HLS_IP_FOLDER  = $(shell readlink -f ./$(NETLAYERDIR)$(NETLAYERHLS)/synthesis_results_HBM)
else ifeq (u2,$(findstring u2, $(DEVICES)))
	HLS_IP_FOLDER  = $(shell readlink -f ./$(NETLAYERDIR)$(NETLAYERHLS)/synthesis_results_noHBM)
endif

LIST_REPOS = --user_ip_repo_paths $(HLS_IP_FOLDER)

$(CMACDIR)$(TEMP_DIR)/%.xo:
	make -C $(CMACDIR) all DEVICES=$(DEVICES) INTERFACE=$(INTERFACE)

$(NETLAYERDIR)$(TEMP_DIR)/%.xo:
	make -C $(NETLAYERDIR) all DEVICES=$(DEVICES)


# BINARY_LINK_OBJS    += --connectivity.nk cmac_1:1
# BINARY_LINK_OBJS    += --connectivity.nk networklayer:1
# BINARY_LINK_OBJS    += --connectivity.slr cmac_1:SLR2
# BINARY_LINK_OBJS    += --connectivity.slr networklayer_1:SLR2

include utils/main.mk