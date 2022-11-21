// Copyright (C) FPGA-FAIS at Jagiellonian University Cracow
//
// SPDX-License-Identifier: BSD-3-Clause



#include "alveo_vnx_cmac.h"
#include "unistd.h"

/**
* AlveoVnxCmac::AlveoVnxCmac() - class constructor
*
* @param device
*  xrt::device, particular Alveo device type to connect to
* @param uuid
*  xrt::uuid, unique ID of the Alveo device
* @param inst_id
*  uint32_t, instance id
*
* Creates an object representing VNX CMAC IP
*/
AlveoVnxCmac::AlveoVnxCmac(const FpgaDevice &device, uint32_t inst_id) :
        FpgaIP::FpgaIP(device, "cmac_" + std::to_string(inst_id)) {
    this->registers_map["gt_reset_reg"] = 0x0000;
    this->registers_map["reset_reg"] = 0x0004;
    this->registers_map["stat_rx_status"] = 0x0204;
    this->registers_map["stat_rx_total_packets"] = 0x0608;
}

/**
* AlveoVnxCmac::AlveoVnxCmacReset()
* Used to reset cmac module.
*/
int AlveoVnxCmac::AlveoVnxCmacReset() {
    this->writeRegister("gt_reset_reg", 1);
    this->writeRegister("reset_reg", 1);
    usleep(1000);
    this->writeRegister("gt_reset_reg", 0);
    this->writeRegister("reset_reg", 0);
    return 0;
}

/**
* AlveoVnxCmac::AlveoVnxCmacReset()
* Used to reset cmac module.
*/
uint32_t AlveoVnxCmac::getCmacPackets() {
    uint32_t number_packets = this->readRegister("stat_rx_total_packets");
    return number_packets;
}