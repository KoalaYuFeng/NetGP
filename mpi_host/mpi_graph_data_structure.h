#ifndef __MPI_GRAPH_DATA_STRUCTURE_H__
#define __MPI_GRAPH_DATA_STRUCTURE_H__

#include <cstring>
#include <vector>
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define MAX_PARTITIONS_NUM      (128)

typedef struct
{
    xrt::kernel gsKernel[SUB_PARTITION_NUM];
    xrt::kernel applyKernel;
    xrt::kernel syncKernel;
    xrt::kernel mergeKernel[SUB_PARTITION_NUM];
    xrt::kernel forwardKernel[SUB_PARTITION_NUM];
    xrt::kernel readKernel[SUB_PARTITION_NUM];
    xrt::kernel writeKernel[SUB_PARTITION_NUM];

    xrt::run gsRun[SUB_PARTITION_NUM];
    xrt::run applyRun;
    xrt::run syncRun;
    xrt::run mergeRun[SUB_PARTITION_NUM];
    xrt::run forwardRun[SUB_PARTITION_NUM]; // for root node, an extra forward kernel for apply
    xrt::run readRun[SUB_PARTITION_NUM];
    xrt::run writeRun[SUB_PARTITION_NUM]; // same as forward kernel 

    std::vector<std::vector<xrt::bo>> edgeBuffer; // each subpartition owns one buffer
    std::vector<std::vector<xrt::bo>> tempBuffer; // for GS kernel temp result, could be optimized.
    xrt::bo propBuffer[SUB_PARTITION_NUM]; // each subpartition owns one buffer
    std::vector<std::vector<xrt::bo>> subBuffer; // sub-buffers of propBuffer
    xrt::bo propBufferNew[SUB_PARTITION_NUM]; // need to divide this buffer to several sub-buffers
    std::vector<std::vector<xrt::bo>> subNewBuffer; // sub-buffers of propBufferNew
    
    xrt::bo outDegBuffer; // each partition owns one buffer
    xrt::bo outRegBuffer; // each partition owns one buffer
    xrt::bo propApplyBuffer; // used for apply stage
    xrt::bo propApplyBufferNew; // updated by forward kernel.

    xrt::device graphDevice;
    xrt::uuid graphUuid;

} graphAccelerator;

#endif /* __MPI_GRAPH_DATA_STRUCTURE_H__ */
