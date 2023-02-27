
// #include "host_graph_sw.h"
// #include "host_graph_scheduler.h"
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "mpi_graph_api.h"
#include "mpi_host.h"
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define EDEG_MEMORY_SIZE        ((edgeNum + (ALIGN_SIZE * 4) * 128) * 1)
#define VERTEX_MEMORY_SIZE      (((vertexNum - 1)/MAX_VERTICES_IN_ONE_PARTITION + 1) * MAX_VERTICES_IN_ONE_PARTITION)

int getPartitionNum (std::string dataset_name, int sub_partition_num) {
    int p_index = 0;
    int sp_index = 0;
    for (int p = 0; p < 100; p++) {
        for (int sp = 0; sp < 100; sp ++) {
            std::string file_name = dataset_name + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".txt";
            // std::cout << file_name << std::endl;
            std::ifstream find_file(file_name.c_str());
            if (find_file.good()) {
                p_index = (p > p_index)? p: p_index;
                sp_index = (sp > sp_index)? sp: sp_index;
            }
        }
    }
    if (sub_partition_num != (sp_index + 1)) { // double check input txt file.
        std::cout << "[ERROR] SUBPARTITION TXT FILE NOT ALIGN !" <<std::endl;
    }
    return (p_index + 1);
}

int getTxtSize (std::string file_name) {
    // std::cout << "[INFO] Getting file size ... " << file_name << " " ;
    std::fstream file;
    int line_num = 0;
    file.open(file_name, std::ios::in);
    if (file.is_open()) {
        std::string temp;
        while(getline(file, temp)) {
            line_num ++;
        }
    }
    file.close();
    // std::cout << line_num << std::endl;
    return line_num;
}

void schedulerFunction(const std::string &gName, const std::string &mode, graphInfo *info)
{
    std::cout << "[INFO] task scheduler, load order file ... " << std::endl;

    info->order.resize(info->partitionNum);
    for (int i = 0; i < info->partitionNum; i++) {
        info->order[i].resize(SUB_PARTITION_NUM);
    }

    std::string file_name = mode + gName + "/order.txt";
    std::fstream file;
    file.open(file_name, std::ios::in);
    if (file.is_open()) {
        std::string temp_line, temp_word;
        int line = 0;
        while(getline(file, temp_line)) {
            std::stringstream ss(temp_line);
            int i = 0;
            while (getline(ss, temp_word, ' ')) {
                info->order[line][i] = stoi(temp_word);
                i += 1;
            }
            line += 1;
        }
    }
    file.close();
}

int acceleratorDataLoad(const std::string &gName, const std::string &mode, graphInfo *info)
{
    int num_partition = getPartitionNum(mode + gName, SUB_PARTITION_NUM);
    info->partitionNum = num_partition;

    int vertex_num = getTxtSize(mode + gName + "/index_mapping.txt");
    info->vertexNum = vertex_num;

    // read vertex mapping file and outdeg file.
    std::fstream mapping_file;
    std::fstream out_deg_file;
    int num_mapped = 0;
    mapping_file.open(mode + gName + "/index_mapping.txt", std::ios::in);
    out_deg_file.open(mode + gName + "/outDeg.txt", std::ios::in);
    if (mapping_file.is_open() && out_deg_file.is_open()) {
        std::string mapping_t, out_deg_t;
        int int_map_t, int_deg_t;
        int line_num_t = 0;
        while ((getline(mapping_file, mapping_t)) && (getline(out_deg_file, out_deg_t))) {
            line_num_t++;
            int_map_t = stoi(mapping_t);
            int_deg_t = stoi(out_deg_t);
            if ((int_map_t != 2147483647) && (int_deg_t != 0)) { // predefine 2147483647 as a invalid value
                info->outDeg.push_back(int_deg_t);
                info->vertexMapping.push_back(int_map_t);
                num_mapped++;
            } else if ((int_map_t == 2147483647) && (int_deg_t == 0)) {
                info->outDegZeroIndex.push_back(line_num_t);
            } else {
                std::cout << "[ERROR] Mapping file not equals to Outdeg file!" << std::endl;
            }
        }
        if (line_num_t != vertex_num) { // double check
            std::cout << "[ERROR] Mapping and OutDeg file size not align!" << std::endl;
        }
    } else {
        std::cout << "[ERROR] Index and Mapping file can not open!" << std::endl;
        return 0;
    }
    std::cout << "[INFO] load mapping and outdeg files done! " << std::endl;

    info->compressedVertexNum = num_mapped;
    int aligned_vertex_num = ((num_mapped + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
    info->alignedCompressedVertexNum = aligned_vertex_num;

    info->chunkProp.resize(num_partition);
    info->chunkEdgeData.resize(num_partition);
    info->chunkTempData.resize(num_partition);
    info->chunkPropData.resize(SUB_PARTITION_NUM);
    info->chunkPropDataNew.resize(SUB_PARTITION_NUM);
    for (int i = 0; i < num_partition; i++) {
        info->chunkProp[i].resize(SUB_PARTITION_NUM);
        info->chunkEdgeData[i].resize(SUB_PARTITION_NUM);
        info->chunkTempData[i].resize(SUB_PARTITION_NUM);
    }

    for (int p = 0; p < num_partition; p++) {
        std::cout << "[INFO] Allocate edge mem space in Host side ... partition : " << p << std::endl;
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            std::string edge_txt_name = mode + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".txt";
            int edge_num = getTxtSize(edge_txt_name);
            info->chunkProp[p][sp].edgeNumChunk = edge_num;
            info->chunkEdgeData[p][sp] = new int[edge_num * 2]; // each edge has 2 vertics.
            info->chunkPropData[sp] = new int[info->alignedCompressedVertexNum];
            info->chunkPropDataNew[sp] = new int[info->alignedCompressedVertexNum];
            info->chunkTempData[p][sp] = new int[PARTITION_SIZE];

            std::fill_n(info->chunkTempData[p][sp], PARTITION_SIZE, 0);
            std::fill_n(info->chunkPropData[sp], info->alignedCompressedVertexNum, 0);
            std::fill_n(info->chunkPropDataNew[sp], info->alignedCompressedVertexNum, 0);
            std::fill_n(info->chunkEdgeData[p][sp], edge_num * 2, 0);
        }
    }

    info->chunkOutDegData = new int[info->alignedCompressedVertexNum];
    info->chunkOutRegData = new int[info->alignedCompressedVertexNum];
    info->chunkApplyPropData = new int[info->alignedCompressedVertexNum];
    std::fill_n(info->chunkOutDegData, info->alignedCompressedVertexNum, 0);
    std::fill_n(info->chunkOutRegData, info->alignedCompressedVertexNum, 0);
    std::fill_n(info->chunkApplyPropData, info->alignedCompressedVertexNum, 0);

    schedulerFunction(gName, mode, info); // load scheduler txt order file.

    return 0;
}

void partitionFunction(const std::string &gName, const std::string &mode, graphInfo *info)
{
    std::cout << "[INFO] load edge txt file after partition ... " << std::endl;

    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            int order_idx = (USE_SCHEDULER == true)? info->order[p][sp] : sp;
            std::string file_name = mode + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(order_idx) + ".txt";
            std::fstream file;
            file.open(file_name, std::ios::in);
            if (file.is_open()) {
                std::string tp, s_tp;
                int i = 0, line = 0;
                int data_t[2];
                while(getline(file, tp)) {
                std::stringstream ss(tp);
                    while (getline(ss, s_tp, ' ')) {
                        data_t[i] = stoi(s_tp);
                        i = (i + 1) % 2;
                    }
                    if (data_t[1] == 2147483646) {
                        info->chunkEdgeData[p][sp][line*2] = data_t[0];
                        info->chunkEdgeData[p][sp][line*2 + 1] = ENDFLAG - 1;
                    } else {
                        info->chunkEdgeData[p][sp][line*2] = data_t[0];
                        info->chunkEdgeData[p][sp][line*2 + 1] = data_t[1];
                    }
                    line++;
                }
                file.close(); // close the file object.
            } else {
                std::cout << "[ERROR] can not open edge list file " << file_name << std::endl;
            }
        }
    }

    std::cout << "[INFO] load edge txt file done ! " << std::endl;
}

int acceleratorDataPreprocess(const std::string &gName, const std::string &mode, graphInfo *info)
{
    // schedulerRegister();
    dataPrepareProperty(info);
    partitionFunction(gName, mode, info);
    return 0;
}
