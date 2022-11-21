// Copyright (C) FPGA-FAIS at Jagiellonian University Cracow
//
// SPDX-License-Identifier: BSD-3-Clause


#pragma once


/**
 * @brief UDP Payload size (max 65536 reduced by header and rounded for 512b word alignment)
 * 
 */
// #define MAX_UDP_BUFFER_SIZE 65532
#define MAX_UDP_BUFFER_SIZE 131068 // since the size of header is 4 byte, so need to be 128*n - 4, like 124, 252....

#define SIZE_RX_BUFFER 4194304 // Set RX buffer as 4M bytes, for max file data receive.

#define SIZE_TX_DATA 4194176 // 32 MAX_UDP_BUFFER_SIZE