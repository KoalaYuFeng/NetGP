#ifndef __MPI_GRAPH_API_H__
#define __MPI_GRAPH_API_H__

#include <string>
#include <vector>
#include "mpi_graph_data_structure.h"

typedef struct
{
    int edgeNumChunk; // edge number of a data chunk
} chunkInfo;

typedef struct
{
    int vertexNum; // original vertex;
    int edgeNum; // original edge number;

    int compressedVertexNum;// delete vetices whose outdeg equal to 0;
    int alignedCompressedVertexNum; // make compressed vertex number aligned to 1024;
    int compressedEdgeNum; // delete edge whose dest vertics'outdeg = 0;
    std::vector<int> outDeg; // compressed vertex out degree
    std::vector<int> vertexMapping; // vertex mapping function, (compressed -> original)
    std::vector<int> outDegZeroIndex; // store vertex index whose ourDeg = 0;

    std::vector<std::vector<int>> order; // used for task scheduling.

    int partitionNum; // partition number;
    std::vector<std::vector<chunkInfo>> chunkProp; // partition -> subpartition -> prop
    std::vector<std::vector<int*>> chunkEdgeData; // partition -> subpartition -> edgelist;
    std::vector<std::vector<int*>> chunkTempData; // temp data, partition -> subpartition.
    std::vector<int*> chunkPropData; // prop data, each subpartition owns whole vertex data.
    std::vector<int*> chunkPropDataNew; // need to do ping-pong operation with old data in each superstep.

    int* chunkOutDegData;
    int* chunkOutRegData;
    int* chunkApplyPropData;

} graphInfo;

#endif /* __MPI_GRAPH_API_H__ */
