#ifndef __MPI_HOST_H__
#define __MPI_HOST_H__

#include <string>
#include <vector>

#include "mpi_graph_api.h"
#include "../utils/log/log.h"

#define ENDFLAG                 0xffffffff
#define USE_APPLY               true
#define SUPERSTEP_NUM           10
#define GRAPH_DATASET_DIRETORY  "/data/yufeng/graph_partition/graph_dataset_sub_change_"

void partitionTransfer(graphInfo *info, graphAccelerator *acc);
void setAccKernelArgs(graphInfo *info, graphAccelerator * acc);
int acceleratorInit(graphInfo *info, graphAccelerator* acc);
int acceleratorDataLoad(const std::string &gName, graphInfo *info);
int acceleratorDataPreprocess(const std::string &gName, graphInfo *info);
int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator * acc);
int dataPrepareProperty(graphInfo *info);

#endif /* __MPI_HOST_H__ */ 