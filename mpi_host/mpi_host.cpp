#include "cmdlineparser.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <cstdlib>

#include "host_graph_api.h"
#include "mpi_host.h"
#include "mpi_network.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

// #define PROCESSOR_NUM 2 // for 2 processors;

int GS_Execute (int id, int partition, graphInfo *info, graphAccelerator * acc) {

    std::cout << "GS Execute at processor " << id << " partition " << partition << " begin ...";

    if (partition < 0) return -1; // check the right parition id;

    int p = partition;
    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        int isp = id + sp * PROCESSOR_NUM;
        acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
        acc->gsRun[sp].set_arg(1, acc->propBuffer[sp]);
        acc->gsRun[sp].set_arg(2, acc->tempBuffer[p][sp]);
        acc->gsRun[sp].set_arg(3, info->chunkProp[p][isp].edgeNumChunk * 2);
        acc->gsRun[sp].start();
    }

    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        acc->gsRun[sp].wait();
    }

    std::cout << " ... end" << std::endl;
    return partition;
}

int main(int argc, char** argv) {

    MPI_Init(NULL, NULL);

    int world_rank; // assume rank = 0 is the root node.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::cout << "[INFO] MPI init done: " << world_rank << "/" << world_size << std::endl;

    graphInfo graphDataInfo;
    graphAccelerator thunderGraph;

    sda::utils::CmdLineParser parser;
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.parse(argc, argv);

    // Read settings
    std::string binary_file;
    if (world_rank == 0) { // root node
        binary_file = "./acc_root.xclbin";
    } else if (world_rank == (world_size - 1)) { // last node;
        binary_file = "./acc_last.xclbin";
    } else { // middle nodes
        binary_file = "./acc_node_" + std::to_string(world_rank) + ".xclbin";
    }
    std::string g_name = parser.value("dataset");
    std::string path_graph_dataset = "/data/binary_graph_dataset/";
    std::cout << "[INFO] Start main" << std::endl;

    // load graph
    acceleratorDataLoad(g_name, path_graph_dataset, &graphDataInfo); // each processor holds full graph.

    // init accelerator : init network stack, init kernel , init buffer , map host and device memory
    acceleratorInit(world_rank, world_size, binary_file, &graphDataInfo, &thunderGraph);

    // graph data pre-process
    acceleratorDataPreprocess(&graphDataInfo); // data prepare + data partition
    // acceleratorCModelDataPreprocess(&graphDataInfo); // cmodel data preprocess, for verification

    // transfer host data to FPGA side
    partitionTransfer(world_rank, &graphDataInfo, &thunderGraph);

    // set acclerator kernel args.
    setAccKernelArgs(world_rank, world_size, &graphDataInfo, &thunderGraph);

    // need to sync before super step execution

    int p_temp_rx[PROCESSOR_NUM];
    int sync_flag = 0;
    for (int i = 0; i < PROCESSOR_NUM; i++) {
        p_temp_rx[i] = -1;
    }
    MPI_Gather(&(sync_flag), 1, MPI_INT, p_temp_rx, 1, MPI_INT, 0, MPI_COMM_WORLD); // * sync operation *

    // run super steps
    int super_step = 10; // run 10 super steps.
    auto start_kernel = chrono::steady_clock::now();

    for (int s = 0; s < super_step; s++) {
        for (int p_idx = 0; p_idx < graphDataInfo.partitionNum; p_idx++) { // for each partition
            int result = GS_Execute(world_rank, p_idx, &graphDataInfo, &thunderGraph); // proc_id, partition_id, graph_info, acc_info;
            // std::cout << "Sync partition [" << p_idx << "] ...";
            int p_temp_rx[PROCESSOR_NUM];
            for (int i = 0; i < PROCESSOR_NUM; i++) {
                p_temp_rx[i] = -1;
            }
            MPI_Gather(&(result), 1, MPI_INT, p_temp_rx, 1, MPI_INT, 0, MPI_COMM_WORLD); // * sync operation *
            // std::cout << " Done." << std::endl;
            if (world_rank == 0) { // for root node
                bool align = true;
                for (int i = 0; i < PROCESSOR_NUM - 1; i++) {
                    if (p_temp_rx[i] != p_temp_rx[i+1]) align = false;
                }
                if (!align) {
                    std::cout << "[INFO] ASYNC occur in nodes" << std::endl;
                    for (int i = 0; i < PROCESSOR_NUM; i++) {
                        std::cout << "Node[" << i << "] 's p_temp_rx is " << p_temp_rx[i] << std::endl;
                    }
                }
            }
        }
    }

    auto end = chrono::steady_clock::now();
    std::cout << "Graph kernel process elapses " << (chrono::duration_cast<chrono::microseconds>(end - start_kernel).count())/super_step<< "us" << std::endl;

    // need to add result transfer function

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
