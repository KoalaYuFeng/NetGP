
#include "host_graph_sw.h"
#include "host_graph_scheduler.h"
#include <map>
#include <algorithm>
#include <fstream>
#include <cstring>
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

// #define EDEG_MEMORY_SIZE        ((edgeNum + (ALIGN_SIZE * 4) * 128) * 1)
#define VERTEX_MEMORY_SIZE      (((vertexNum - 1)/MAX_VERTICES_IN_ONE_PARTITION + 1) * MAX_VERTICES_IN_ONE_PARTITION)


int acceleratorDataLoad(const std::string &gName, const std::string &mode, graphInfo *info)
{

    // at this time, no need to load binary file

    // int read_size= 0;
    // std::string name;
    // // read csr file : input row point array
    // name = mode + gName + "_rapi.bin";
    // std::ifstream fFileReader_rapi(name, std::ios::binary);
	// if (!fFileReader_rapi) {
	// 	std::cout << "graph rapi read file failed" << std::endl;
	// 	return -1;
	// }
    

    // load index mapping list and outDeg list first;
    std::fstream index_map_list;
    std::fstream outDeg_list;
    std::string filename;

    filename = mode + gName + "/index_mapping.txt";
    index_map_list.open(filename, std::ios::in);
    int num_mapped = 0;
    int num_deleted = 0;
    if (index_map_list.is_open()){ //checking whether the file is open
        std::string temp;
        while(getline(index_map_list, temp)) {
            int mapping_t = stoi(temp);
            if (mapping_t == 2147483647) { // no mapping, set value to 0x7fffffff
                num_deleted ++;
            } else {
                num_mapped ++;
            }
            info->vertexMapping.push_back(mapping_t);
        }
        index_map_list.close(); //close the file object.
    } else {
        std::cout << "[ERROR] Can not open index mapping file" << std::endl;
        return -1;
    }
    info->vertexNum = num_mapped + num_deleted;
    info->compressedVertexNum = num_mapped;
    std::cout << "[INFO] compressed vertex number " << num_mapped << " deleted vertex number " << num_deleted << std::endl;
    std::cout << "[INFO] Overall vertex number " << info->vertexNum << std::endl;
    
    filename = mode + gName + "/outDeg.txt";
    outDeg_list.open(filename, std::ios::in);
    if (outDeg_list.is_open()){ //checking whether the file is open
        std::string temp;
        int double_check = 0;
        while(getline(outDeg_list, temp)) {
            int deg_t = stoi(temp);
            if (deg_t != 0) {
                info->outDeg.push_back(deg_t);
                double_check ++;
            }
        }
        outDeg_list.close(); //close the file object.
        if (double_check != num_mapped) {
            std::cout << "[ERROR] vertex number not align" << std::endl;
            return -1;
        }
    } else {
        std::cout << "[ERROR] Can not open outDeg file" << std::endl;
        return -1;
    }

    int aligned_vertex_num = ((num_mapped + 1023)/1024)*1024;
    info->alignedCompressedVertexNum = aligned_vertex_num;

    // calculate partition number and edge number in each subpartition
    int p_index = 0;
    int sp_index = 0;
    int max_index = 100; // search range
    for (int p = 0; p < max_index; p++) {
        for (int sp = 0; sp < max_index; sp ++) {
            std::string file_name = mode + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".txt";
            std::ifstream find_file(file_name.c_str());
            if (find_file.good()) {
                p_index = (p > p_index)? p: p_index;
                sp_index = (sp > sp_index)? sp: sp_index;
            }
        }
    }
    int p_num = p_index + 1;
    int sp_num = sp_index + 1;
    if (sp_num != SUB_PARTITION_NUM) { // double check
        std::cout << "[ERROR] subpartition number not align ! " << sp_num << ' ' << SUB_PARTITION_NUM << std::endl;
    }
    std::cout << "[INFO] partition num " << p_num << " subpartition num " << sp_num << std::endl;

    // check whether file is in the directory
    for (int p = 0; p < p_num; p++) {
        for (int sp = 0; sp < sp_num; sp ++) {
            std::string file_name = mode + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".txt";
            std::ifstream find_file(file_name.c_str());
            if (!find_file.good()) {
                std::cout << "[ERROR] file Not exist " << p << " " << sp << std::endl; 
            }

            if ((sp==0) && (find_file.good())) {
                int t = 0;
                std::string tp;
                while (getline(find_file, tp)) {
                    t++;
                }
                info->edgeNum.push_back(t);
            }
        }
        std::cout << "[INFO] edge num, p_" << p << " : " << info->edgeNum[p] << std::endl;
    }

    info->partitionNum = p_num;
    int num_partition = p_num;
    int num_subpartition = SUB_PARTITION_NUM;

    info->chunkProp.resize(num_partition);
    info->chunkEdgeData.resize(num_partition);

    info->chunkOutDegData = new int[info->alignedCompressedVertexNum];
    info->chunkOutRegData = new int[info->alignedCompressedVertexNum];
    info->chunkApplyPropData = new int[info->alignedCompressedVertexNum];
    std::fill_n(info->chunkOutDegData, info->alignedCompressedVertexNum, 0);
    std::fill_n(info->chunkOutRegData, info->alignedCompressedVertexNum, 0);
    std::fill_n(info->chunkApplyPropData, info->alignedCompressedVertexNum, 0);

    info->chunkTempData.resize(num_partition);
    info->chunkPropData.resize(SUB_PARTITION_NUM);
    info->chunkPropDataNew.resize(SUB_PARTITION_NUM);

    for (int i = 0; i < num_partition; i++) {
        info->chunkProp[i].resize(num_subpartition);
        info->chunkEdgeData[i].resize(num_subpartition);
        info->chunkTempData[i].resize(num_subpartition);
    }

    for (int p = 0; p < num_partition; p++) {

        int edge_num_tmp = info->edgeNum[p];

        for (int sp = 0; sp < num_subpartition; sp++) {
            info->chunkProp[p][sp].edgeNumChunk = edge_num_tmp;
            info->chunkProp[p][sp].destVertexNumChunk = PARTITION_SIZE; // aligned vertex number

            info->chunkEdgeData[p][sp] = new int[edge_num_tmp * 2]; // each edge has 2 vertics.
            info->chunkTempData[p][sp] = new int[PARTITION_SIZE];
            info->chunkPropData[sp] = new int[info->partitionNum * PARTITION_SIZE];
            info->chunkPropDataNew[sp] = new int[info->partitionNum * PARTITION_SIZE];

            std::fill_n(info->chunkTempData[p][sp], PARTITION_SIZE, 0);
            std::fill_n(info->chunkPropData[sp], info->partitionNum * PARTITION_SIZE, 0);
            std::fill_n(info->chunkPropDataNew[sp], info->partitionNum * PARTITION_SIZE, 0);
            std::fill_n(info->chunkEdgeData[p][sp], edge_num_tmp * 2, 0);
        }

    }

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


void partitionFunction(const std::string &gName, const std::string &mode, graphInfo *info)
{
    std::cout << "[INFO] Start partition function " << std::endl;

    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            std::string file_name = mode + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".txt";
            std::cout << "[INFO] Load " << file_name << std::endl;
            std::fstream edgefile;
            edgefile.open(file_name, std::ios::in);
            if (edgefile.is_open()) {
                std::string tp, s_tp;
                int line = 0, i = 0;
                int data_t[2];
                while(getline(edgefile, tp)) {
                    std::stringstream ss(tp);
                    while (std::getline(ss, s_tp, ' ')) {
                        if (stoi(s_tp) == 2147483646) { // 0x7ffffffff-1; END_FLAG
                            data_t[i] = ENDFLAG - 1;
                        }
                        data_t[i] = stoi(s_tp);
                        i = (i + 1) % 2;
                    }
                    info->chunkEdgeData[p][sp][line*2] = data_t[0];
                    info->chunkEdgeData[p][sp][line*2 + 1] = data_t[1];
                    line++;
                }
                edgefile.close(); //close the file object.
                if (line != info->edgeNum[p]) { // double check
                    std::cout << "[ERROR] read partition file not align " << std::endl;
                }
            } else {
                std::cout << "can not open file" << std::endl;
            }
        }
    }

    // ===============  Print information  ================
    // for (int p = 6; p < info->partitionNum; p++) {
    //     for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //         for (int ii = 0; ii < info->chunkProp[p][sp].edgeNumChunk * 2; ii++) {

    //             std::cout << "E[" << p << "][" << sp << "][" << ii << "]:" << info->chunkEdgeData[p][sp][ii] << " ";
    //             if ((ii+1) % 2 == 0) std::cout << std::endl;
    //             // if ((ii % 2 == 1) && (info->chunkEdgeData[p][sp][ii] == 1)) {
    //             //     std::cout << "edge :"<< info->chunkEdgeData[p][sp][ii - 1] <<" "<< info->chunkEdgeData[p][sp][ii]<<" ";
    //             //     std::cout << "prop :"<< info->chunkPropData[sp][info->chunkEdgeData[p][sp][ii - 1]];
    //             //     std::cout << std::endl;
    //             // }
    //             // std::cout << "["<<p<<"]["<<sp<<"]["<<ii<<"]:"<<info->chunkEdgeData[p][sp][ii]<<" ";
    //             // if ((ii + 1) % 2 == 0) std::cout<<std::endl;
    //         }
    //     }
    // }

    // for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //     for (int i = 0; i < info->alignedCompressedVertexNum; i++) {
    //         std::cout << "[" << i <<"] "<< info->chunkPropData[sp][i] << " ";
    //         if ((i+1) % 10 == 0) std::cout<<std::endl;
    //     }
    // }

}

int acceleratorDataPreprocess(const std::string &gName, const std::string &mode, graphInfo *info)
{
    // schedulerRegister();
    dataPrepareProperty(info);
    partitionFunction(gName, mode, info);
    return 0;
}
