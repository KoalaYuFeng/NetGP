#ifndef __MPI_HOST_H__
#define __MPI_HOST_H__

#include <string>
#include <vector>

#include "host_graph_api.h"

int acceleratorInit(int world_rank, std::string& file_name, graphInfo *info, graphAccelerator *acc);
int acceleratorDataLoad(const std::string &gName, const std::string &mode, graphInfo *info);
int acceleratorDataPreprocess(graphInfo *info);
int acceleratorSuperStep(int superStep, graphInfo *info, graphAccelerator *acc);

#endif /* __MPI_HOST_H__ */