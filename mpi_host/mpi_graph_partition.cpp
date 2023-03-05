
// #include "host_graph_sw.h"
// #include "host_graph_scheduler.h"
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <mpi.h>
#include "mpi_graph_api.h"
#include "mpi_host.h"
// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

int getPartitionNum (std::string dataset_name, int sub_partition_num) {
    int p_index = 0;
    int sp_index = 0;
    for (int p = 0; p < 100; p++) {
        for (int sp = 0; sp < 100; sp ++) {
            std::string file_name = dataset_name + "/p_" + std::to_string(p) + "_sp_" + std::to_string(sp) + ".txt";
            std::ifstream find_file(file_name.c_str());
            if (find_file.good()) {
                p_index = (p > p_index)? p: p_index;
                sp_index = (sp > sp_index)? sp: sp_index;
            }
        }
    }
    if (sub_partition_num != (sp_index + 1)) { // double check input txt file.
        log_error("[ERROR] SUBPARTITION TXT FILE NOT ALIGN !");
    }
    return (p_index + 1);
}

int getTxtSize (std::string file_name) {
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
    return line_num;
}

int acceleratorDataLoad(const std::string &gName, graphInfo *info)
{
    std::string directory = GRAPH_DATASET_DIRETORY + std::to_string(SUB_PARTITION_NUM) + "/";
    log_debug("Load grapg data from directory : %s", directory.c_str());

    int num_partition = getPartitionNum(directory + gName, SUB_PARTITION_NUM);
    info->partitionNum = num_partition;

    int vertex_num = getTxtSize(directory + gName + "/index_mapping.txt");
    info->vertexNum = vertex_num;

    // read vertex mapping file and outdeg file.
    std::fstream mapping_file;
    std::fstream out_deg_file;
    int num_mapped = 0;
    mapping_file.open(directory + gName + "/index_mapping.txt", std::ios::in);
    out_deg_file.open(directory + gName + "/outDeg.txt", std::ios::in);
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
                log_error("[ERROR] Mapping file not equals to Outdeg file!");
            }
        }
        if (line_num_t != vertex_num) { // double check
            log_error("[ERROR] Mapping and OutDeg file size not align!");
        }
    } else {
        log_error("[ERROR] Index and Mapping file can not open!");
        return 0;
    }
    log_trace("[TRACE] load mapping and outdeg files done! ");


    // read scheduler order file
    log_trace("[INFO] task scheduler, load order file ... ");
    info->order.resize(info->partitionNum);
    for (int i = 0; i < info->partitionNum; i++) {
        info->order[i].resize(SUB_PARTITION_NUM);
    }
    std::fstream order_file;
    order_file.open(directory + gName + "/order.txt", std::ios::in);
    if (order_file.is_open()) {
        std::string temp_line, temp_word;
        int line = 0;
        while(getline(order_file, temp_line)) {
            std::stringstream ss(temp_line);
            int i = 0;
            while (getline(ss, temp_word, ' ')) {
                info->order[line][i] = stoi(temp_word);
                i += 1;
            }
            line += 1;
        }
    }
    order_file.close();
    log_trace("[TRACE] load scheduler order files done! ");


    // Allocate edge mem space in Host side
    info->compressedVertexNum = num_mapped;
    int aligned_vertex_num = ((num_mapped + PARTITION_SIZE - 1) / PARTITION_SIZE) * PARTITION_SIZE;
    info->alignedCompressedVertexNum = aligned_vertex_num;
    log_debug("whole graph compressed vertex num %d, algned %d", num_mapped, aligned_vertex_num);


    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    info->chunkProp.resize(num_partition);
    info->chunkEdgeData.resize(num_partition);
    info->chunkPropData.resize(kernel_per_node);
    info->chunkPropDataNew.resize(kernel_per_node);
    for (int i = 0; i < num_partition; i++) {
        info->chunkProp[i].resize(kernel_per_node);
        info->chunkEdgeData[i].resize(kernel_per_node);
    }

    for (int p = 0; p < num_partition; p++) {
        log_trace("Allocate mem space in Host side ... partition : %d", p);
        for (int sp = 0; sp < kernel_per_node; sp++) {
            int order_idx = (USE_SCHEDULER == true)? info->order[p][sp + world_rank*kernel_per_node] : (sp + world_rank*kernel_per_node);
            std::string edge_txt_name = directory + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(order_idx) + ".txt";
            int edge_num = getTxtSize(edge_txt_name);
            info->chunkProp[p][sp].edgeNumChunk = edge_num;
            info->chunkEdgeData[p][sp] = new int[edge_num * 2]; // each edge has 2 vertics.
            info->chunkPropData[sp] = new int[info->alignedCompressedVertexNum];
            info->chunkPropDataNew[sp] = new int[info->alignedCompressedVertexNum];
        }
    }

    info->chunkOutDegData = new int[info->alignedCompressedVertexNum];
    info->chunkOutRegData = new int[info->alignedCompressedVertexNum];
    info->chunkApplyPropData = new int[info->alignedCompressedVertexNum];

    log_trace("Allocate mem space in Host side done!");
    return 0;
}


int acceleratorDataPreprocess(const std::string &gName, graphInfo *info)
{
    dataPrepareProperty(info); // function in each application directory

    log_trace("[INFO] load edge txt file after partition ... ");
    std::string directory = GRAPH_DATASET_DIRETORY + std::to_string(SUB_PARTITION_NUM) + "/";

    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < kernel_per_node; sp++) {
            int order_idx = (USE_SCHEDULER == true)? info->order[p][sp + world_rank*kernel_per_node] : (sp + world_rank*kernel_per_node);
            std::string file_name = directory + gName + "/p_" + std::to_string(p) + "_sp_" + std::to_string(order_idx) + ".txt";
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
                log_error("[ERROR] can not open edge list file %s", file_name.c_str());
            }
        }
    }
    log_trace("[INFO] load edge txt file done !");
    return 0;
}