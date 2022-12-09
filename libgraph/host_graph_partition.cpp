
#include "host_graph_sw.h"
#include "host_graph_mem.h"
#include "host_graph_scheduler.h"
#include <map>
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define EDEG_MEMORY_SIZE        ((edgeNum + (ALIGN_SIZE * 4) * 128) * 1)
#define VERTEX_MEMORY_SIZE      (((vertexNum - 1)/MAX_VERTICES_IN_ONE_PARTITION + 1) * MAX_VERTICES_IN_ONE_PARTITION)


int acceleratorDataLoad(const std::string &gName, const std::string &mode, graphInfo *info)
{
    Graph* gptr = createGraph(gName, mode);
    CSR* csr    = new CSR(*gptr);
    free(gptr);

    int vertex_num = csr->vertexNum;
    int edge_num   = csr->edgeNum;
    info->vertexNum = vertex_num;
    info->edgeNum   = edge_num;
    
    // compress function : delete vertics whose OutDeg = 0;
    int num_mapped = 0;
    int num_deleted = 0;

    int* mapping_vertex_array = new int[vertex_num];
    for (int i = 0; i < vertex_num; i++) {
        mapping_vertex_array[i] = num_mapped;
        int outdeg_tmp = csr->rpao[i + 1] - csr->rpao[i];
        if (outdeg_tmp > 0) {
            info->outDeg.push_back(outdeg_tmp);
            info->vertexMapping.push_back(i);
            num_mapped++;
        } else {
            info->outDegZeroIndex.push_back(i);
            num_deleted++;
        }
    }

    if ((num_mapped + num_deleted) != vertex_num) {
        std::cout << "graph vertex compress error!" << std::endl; // double check
    }

    info->compressedVertexNum = num_mapped;

    int aligned_vertex_num = ((num_mapped + 1023)/1024)*1024;
    info->alignedCompressedVertexNum = aligned_vertex_num;

    std::cout << "Vertex compress: original : "<< vertex_num << " compressed : "<< num_mapped << " aligned: "<< aligned_vertex_num << std::endl;

    // compress function : delete edges whose dest vertex's OutDeg = 0;
    int num_compress_edge = 0;
    info->rpa.resize(num_mapped + 1);
    info->rpa[0] = 0;
    for (int j = 0; j < num_mapped; j++) {
        int in_deg_tmp = csr->rpai[info->vertexMapping[j]+1] - csr->rpai[info->vertexMapping[j]];
        info->rpa[j+1] = info->rpa[j] + in_deg_tmp;
        int bias = csr->rpai[info->vertexMapping[j]];
        int tmp_cia = 0;
        for (int k = 0; k < in_deg_tmp; k++) {
            info->cia.push_back(mapping_vertex_array[csr->ciai[bias + k]]);
            info->destIndexList.push_back(j); // for time optimization in partition function.
            num_compress_edge++;
        }
    }

    if ( num_compress_edge != info->rpa[num_mapped]) {
        std::cout << "graph edge compress error!" << std::endl; // double check
    }

    std::cout << "Edge compress: original : "<< edge_num << " compressed : "<< num_compress_edge << std::endl;
    info->compressedEdgeNum = num_compress_edge;

    // calculate partition number and edge number in each subpartition
    int num_partition = (num_mapped + PARTITION_SIZE - 1) / PARTITION_SIZE; // PARTITION_SIZE align
    int num_subpartition = SUB_PARTITION_NUM;
    info->partitionNum = num_partition;

    int vertex_index_start = 0;
    int vertex_index_end = 0;

    info->chunkProp.resize(num_partition);
    info->chunkEdgeData.resize(num_partition);
    info->chunkOutDegData = new int[info->alignedCompressedVertexNum];
    info->chunkOutRegData = new int[info->alignedCompressedVertexNum];

    info->chunkTempData.resize(SUB_PARTITION_NUM);
    info->chunkPropData.resize(SUB_PARTITION_NUM);

    for (int i = 0; i < num_partition; i++) {
        info->chunkProp[i].resize(num_subpartition);
        info->chunkEdgeData[i].resize(num_subpartition);
    }

    for (int p = 0; p < num_partition; p++) {

        vertex_index_start = p * PARTITION_SIZE;
        vertex_index_end = ((p+1)*PARTITION_SIZE > num_mapped)? num_mapped : (p+1)*PARTITION_SIZE;

        int edge_num_tmp = 0;
        edge_num_tmp = info->rpa[vertex_index_end] - info->rpa[vertex_index_start]; // for each partition
        edge_num_tmp = (edge_num_tmp + num_subpartition - 1) / num_subpartition; // for each subpartition;
        edge_num_tmp = ((edge_num_tmp + ALIGN_SIZE - 1) / ALIGN_SIZE) * ALIGN_SIZE; // for alignment

        for (int sp = 0; sp < num_subpartition; sp++) {
            info->chunkProp[p][sp].edgeNumChunk = edge_num_tmp;
            info->chunkProp[p][sp].destVertexNumChunk = vertex_index_end - vertex_index_start;

            info->chunkEdgeData[p][sp] = new int[edge_num_tmp * 2]; // each edge has 2 vertics.
            info->chunkTempData[sp] = new int[info->alignedCompressedVertexNum];
            info->chunkPropData[sp] = new int[info->alignedCompressedVertexNum];
        }

        std::cout << "Partition "<< p <<" start index "<< vertex_index_start << " end index "<< vertex_index_end << std::endl;
        std::cout << " Edge number: partion "<< edge_num_tmp * num_subpartition << " subpartion "<< edge_num_tmp << std::endl;
    }

    delete [] mapping_vertex_array;
    return 0;
}


void reTransferProp(graphInfo *info)
{
//     dataPrepareProperty(info);
//     graphAccelerator * acc = getAccelerator();

//     int *rpa = (int*)get_host_mem_pointer(MEM_ID_RPA);
//     prop_t *vertexPushinPropMapped = (prop_t*)get_host_mem_pointer(MEM_ID_PUSHIN_PROP_MAPPED);
//     prop_t *vertexPushinProp       = (prop_t*)get_host_mem_pointer(MEM_ID_PUSHIN_PROP);
//     unsigned int *vertexMap        = (unsigned int *)get_host_mem_pointer(MEM_ID_VERTEX_INDEX_MAP);
//     unsigned int *vertexRemap      = (unsigned int *)get_host_mem_pointer(MEM_ID_VERTEX_INDEX_REMAP);
//     unsigned int mapedSourceIndex  = 0;
//     int vertexNum = info->vertexNum;

//     for (int u = 0; u < vertexNum; u++) {
//         int num = rpa[u + 1] - rpa[u];
//         vertexMap[u] = mapedSourceIndex;
//         if (num != 0)
//         {
// #if CAHCE_FETCH_DEBUG
//             vertexPushinPropMapped[mapedSourceIndex] =  mapedSourceIndex;
// #else
//             vertexPushinPropMapped[mapedSourceIndex] =  vertexPushinProp[u];
// #endif
//             vertexRemap[mapedSourceIndex] = u;
//             mapedSourceIndex ++;
//         }
//     }
//     process_mem_init(acc->context);
//     partitionTransfer(info);
}


void partitionFunction(graphInfo *info)
{
    std::cout << "Start partition function " << std::endl;
    int vertex_index_start = 0;
    int vertex_index_end = 0;

    std::vector<std::multimap<int, int>> edge_list;
    edge_list.resize(info->partitionNum);
    
    for (int p = 0; p < info->partitionNum; p++) {

        vertex_index_start = p * PARTITION_SIZE;
        vertex_index_end = ((p+1)*PARTITION_SIZE > (info->compressedVertexNum)) ? (info->compressedVertexNum) : (p+1)*PARTITION_SIZE;

        std::cout << "Partition "<< p <<" start index "<< vertex_index_start << " end index "<< vertex_index_end << std::endl;

        for (int i = info->rpa[vertex_index_start]; i < info->rpa[vertex_index_end]; i++) {
            edge_list[p].insert(std::pair<int, int>(info->cia[i], info->destIndexList[i]));
        }

        for (int i = info->rpa[vertex_index_start]; i < info->rpa[vertex_index_end];) {
            auto it = edge_list[p].cbegin();
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                int max = 0;
                int min = 0;
                for (int ii = 0; ii < info->chunkProp[p][sp].edgeNumChunk; ii++) {
                    if (info->rpa[vertex_index_end] <= (sp*info->chunkProp[p][sp].edgeNumChunk + ii + info->rpa[vertex_index_start])) {
                        // info->chunkEdgeData[p][sp][ii*2] = ENDFLAG - 1; // GS will not process this edge, just for alignment;
                        info->chunkEdgeData[p][sp][ii*2] = info->compressedVertexNum - 1;
                        info->chunkEdgeData[p][sp][ii*2 + 1] = info->compressedVertexNum - 2;
                    } else {
                        info->chunkEdgeData[p][sp][ii*2] = (*it).first; // source vertex
                        info->chunkEdgeData[p][sp][ii*2 + 1] = (*it).second; // dest vertex

                        max = (info->cia[i] > max) ? info->cia[i] : max;
                        min = (info->cia[i] > min) ? min : info->cia[i];

                        i++;
                        it++;
                    }
                }
                info->chunkProp[p][sp].srcVertexNumChunk = max - min;
            }
        }
        std::cout << " Partition " << p << " process done" <<std::endl; 
    }

    // ===============  Print information  ================
    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            for (int ii = 0; ii < info->chunkProp[p][sp].edgeNumChunk * 2; ii++) {
                if ((ii % 2 == 1) && (info->chunkEdgeData[p][sp][ii] == 5428)) {
                    std::cout << "edge :"<< info->chunkEdgeData[p][sp][ii - 1] <<" "<< info->chunkEdgeData[p][sp][ii]<<" ";
                    std::cout << "prop :"<< info->chunkPropData[sp][info->chunkEdgeData[p][sp][ii - 1]];
                    std::cout << std::endl;
                }
                // std::cout << "["<<p<<"]["<<sp<<"]["<<ii<<"]:"<<info->chunkEdgeData[p][sp][ii]<<" ";
                // if ((ii + 1) % 2 == 0) std::cout<<std::endl;
            }
        }
        
    }

    // for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //     for (int i = 0; i < info->alignedCompressedVertexNum; i++) {
    //         std::cout << "[" << i <<"] "<< info->chunkPropData[sp][i] << " ";
    //         if ((i+1) % 10 == 0) std::cout<<std::endl;
    //     }
    // }

    std::cout << " Partition function finish" << std::endl;
}

int acceleratorDataPreprocess(graphInfo *info)
{
    // schedulerRegister();
    dataPrepareProperty(info);
    partitionFunction(info);
    return 0;
}
