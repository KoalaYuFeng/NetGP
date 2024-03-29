#ifndef __HOST_GRAPH_API_H__
#define __HOST_GRAPH_API_H__

#include <string>
#include <vector>

#include "common.h"
#include "global_config.h"
#include "host_graph_data_structure.h"

typedef struct
{
    int edgeNumChunk; // edge number of a data chunk
    int srcVertexNumChunk; // source vertex number of edgeNumChunk
    int destVertexNumChunk; // dest vertex number of edgeNumChunk
} chunkInfo;

typedef struct
{
    int vertexNum; // original vertex;
    // int edgeNum; // original edge number;

    int compressedVertexNum;// delete vetices whose outdeg equal to 0;
    int alignedCompressedVertexNum; // make compressed vertex number aligned to 1024;
    int compressedEdgeNum; // delete edge whose dest vertics'outdeg = 0;
    std::vector<int> outDeg; // compressed vertex out degree
    std::vector<int> vertexMapping; // vertex mapping function, (compressed -> original)
    std::vector<int> outDegZeroIndex; // store vertex index whose ourDeg = 0;

    std::vector<int> destIndexList; // used in partition function for time optimization;

    std::vector<int> rpa; // row point array after compression
    std::vector<int> cia; // column index array after compression

    int partitionNum; // partition number;
    std::vector<int> edgeNum; // store the edge number of each partition
    std::vector<std::vector<chunkInfo>> chunkProp; // partition -> subpartition -> prop
    std::vector<std::vector<int*>> chunkEdgeData; // partition -> subpartition -> edgelist;
    std::vector<std::vector<int*>> chunkTempData; // temp data, partition -> subpartition.
    std::vector<int*> chunkPropData; // prop data, each subpartition owns whole vertex data.
    std::vector<int*> chunkPropDataNew; // need to do ping-pong operation with old data in each superstep.

    int* chunkOutDegData;
    int* chunkOutRegData;
    int* chunkApplyPropData;

} graphInfo;

/* misc */
unsigned int dataPrepareGetArg(graphInfo *info);
int dataPrepareProperty(graphInfo *info);
double getCurrentTimestamp(void);
void reTransferProp(graphInfo *info);
void partitionTransfer(graphInfo *info, graphAccelerator *acc);
void setAccKernelArgs(graphInfo *info, graphAccelerator *acc);
void resultTransfer(graphInfo *info, graphAccelerator *acc, int);

/* host api -- dataflow */
int acceleratorInit(std::string& file_name, graphInfo *info, graphAccelerator *acc);
int acceleratorDataLoad(const std::string &gName, const std::string &mode, graphInfo *info);
int acceleratorDataPreprocess(const std::string &gName, const std::string &mode, graphInfo *info);
int acceleratorSuperStep(int superStep, graphInfo *info, graphAccelerator *acc);
int acceleratorDeinit(void);

/* host api -- query */
void* acceleratorQueryRegister(void);
prop_t* acceleratorQueryProperty(int step);

#endif /* __HOST_GRAPH_API_H__ */
