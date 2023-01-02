#ifndef __MPI_HOST_H__
#define __MPI_HOST_H__

#include <string>
#include <vector>

#include "host_graph_api.h"

void partitionTransfer(int world_rank, graphInfo *info, graphAccelerator *acc);
void setAccKernelArgs(int world_rank, int world_size, graphInfo *info, graphAccelerator * acc);
int acceleratorInit(int world_rank, int world_size, std::string& file_name,  graphInfo *info, graphAccelerator* acc);
int acceleratorDataLoad(const std::string &gName, const std::string &mode, graphInfo *info);
int acceleratorDataPreprocess(graphInfo *info);
int accGatherScatterExecute (int super_step, int world_rank, int partition, graphInfo *info, graphAccelerator * acc);
int accApplyStart (int world_rank, int world_size, graphInfo *info, graphAccelerator * acc);
int accApplyEnd (int world_rank, int world_size, graphInfo *info, graphAccelerator * acc);

#endif /* __MPI_HOST_H__ */ 