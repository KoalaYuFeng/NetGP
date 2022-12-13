#ifndef __HOST_GRAPH_DATA_STRUCTURE_H__
#define __HOST_GRAPH_DATA_STRUCTURE_H__

#include <cstring>
#include <vector>
#include "graph.h"
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define MAX_PARTITIONS_NUM      (128)

typedef struct
{
    xrt::kernel gsKernel[SUB_PARTITION_NUM];
    xrt::kernel applyKernel;
    xrt::kernel mergeKernel[SUB_PARTITION_NUM];
    xrt::kernel forwardKernel[SUB_PARTITION_NUM];
    xrt::kernel readKernel[SUB_PARTITION_NUM];
    xrt::kernel writeKernel[SUB_PARTITION_NUM];

    xrt::run gsRun[SUB_PARTITION_NUM];

    std::vector<std::vector<xrt::bo>> edgeBuffer; // each subpartition owns one buffer
    std::vector<std::vector<xrt::bo>> tempBuffer;
    xrt::bo propBuffer[SUB_PARTITION_NUM]; // each subpartition owns one buffer

    xrt::bo outDegBuffer; // each partition owns one buffer
    xrt::bo outRegBuffer; // each partition owns one buffer

    xrt::device graphDevice;
    xrt::uuid graphUuid;

} graphAccelerator;

#endif /* __HOST_GRAPH_DATA_STRUCTURE_H__ */
