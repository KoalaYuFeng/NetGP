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

int GS_Execute (int super_step, int world_rank, int partition, graphInfo *info, graphAccelerator * acc) {

    std::cout << "GS " << super_step << " Execute at processor " << world_rank << " partition " << partition << " begin ...";

    if (partition < 0) return -1; // check the right parition id;

    int p = partition;
    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        int isp = world_rank + sp * PROCESSOR_NUM;
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

int APPLY_Execution_start (int world_rank, int world_size, graphInfo *info, graphAccelerator * acc) {

    if (world_rank == 0) { // root node
        std::cout << "root apply start exe .. " << std::endl;
        // Apply kernel start
        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].set_arg(0, acc->propBuffer[sp]);
            acc->readRun[sp].set_arg(2, info->alignedCompressedVertexNum);
            acc->readRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->mergeRun[sp].set_arg(3, 0); // dest = 0;
            acc->mergeRun[sp].set_arg(4, info->alignedCompressedVertexNum);
            acc->mergeRun[sp].start();
        }

        acc->applyRun.set_arg(0, acc->propBuffer[0]);
        acc->applyRun.set_arg(5, info->alignedCompressedVertexNum); // depends on which SLR
        acc->applyRun.start();

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->forwardRun[sp].set_arg(3, 1); // dest = 1;
            acc->forwardRun[sp].set_arg(4, info->alignedCompressedVertexNum);
            acc->forwardRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].set_arg(0, acc->propBufferNew[sp]);
            acc->writeRun[sp].set_arg(2, info->alignedCompressedVertexNum);
            acc->writeRun[sp].start();
        }
        std::cout << "root node start done" << std::endl;
    } else if (world_rank < world_size - 1) { // middle node
    
    } else { // last node 
        sleep(5); // wait until root node start done.
        std::cout << "last node apply start exe .. " << std::endl;
        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].set_arg(0, acc->propBufferNew[sp]);
            acc->writeRun[sp].set_arg(2, info->alignedCompressedVertexNum);
            acc->writeRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM) - 1; sp++) {
            acc->forwardRun[sp].set_arg(3, 0); // dest = 0;
            acc->forwardRun[sp].set_arg(4, info->alignedCompressedVertexNum);
            acc->forwardRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].set_arg(0, acc->propBuffer[sp]);
            acc->readRun[sp].set_arg(2, info->alignedCompressedVertexNum);
            acc->readRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM) - 1; sp++) {
            acc->mergeRun[sp].set_arg(3, 1); // dest = 1;
            acc->mergeRun[sp].set_arg(4, info->alignedCompressedVertexNum);
            acc->mergeRun[sp].start();
        }

        std::cout << "last node start done" << std::endl;
    }
    return 0;
}

int APPLY_Execution_end (int world_rank, int world_size, graphInfo *info, graphAccelerator * acc) {

    if (world_rank == 0) { // root node
        std::cout << "root node apply wait ..." << std::endl;
        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].wait();
            std::cout << "root read wait done" << std::endl;
        }
        

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->mergeRun[sp].wait();
            std::cout << "root merge wait done" << std::endl;
        }
        

        acc->applyRun.wait();
        std::cout << "root apply wait done" << std::endl;

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->forwardRun[sp].wait();
            std::cout << "root forward wait done" << std::endl;
        }
        

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].wait();
            std::cout << " root node end done" << std::endl;
        }
        

    } else if (world_rank < world_size - 1) { // middle node 

    } else { //last node

        std::cout << "last node apply wait ..." << std::endl;

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].wait();
            std::cout << "last read wait done" << std::endl;
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM) - 1; sp++) {
            acc->mergeRun[sp].wait();
            std::cout << "last merge wait done" << std::endl;
        }
        

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM) - 1; sp++) {
            acc->forwardRun[sp].wait();
            std::cout << "last forward wait done" << std::endl;
        }
        

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].wait();
            std::cout << "last write wait done" << std::endl;
        }
       
    }

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

    MPI_Barrier(MPI_COMM_WORLD); // sync barrier;

    // run super steps
    int super_step = 10; // run 10 super steps.
    auto start_kernel = chrono::steady_clock::now();

    for (int s = 0; s < super_step; s++) {
        for (int p_idx = 0; p_idx < graphDataInfo.partitionNum; p_idx++) { // for each partition
            int result = GS_Execute(s, world_rank, p_idx, &graphDataInfo, &thunderGraph); // proc_id, partition_id, graph_info, acc_info;
            // std::cout << "Sync partition [" << p_idx << "] ...";

            int p_temp_rx[PROCESSOR_NUM];
            for (int i = 0; i < PROCESSOR_NUM; i++) {
                p_temp_rx[i] = -1;
            }
            MPI_Gather(&(result), 1, MPI_INT, p_temp_rx, 1, MPI_INT, 0, MPI_COMM_WORLD); // * no need to sync, just like aw-barrier *
            // std::cout << " Done." << std::endl;
            // if (world_rank == 0) { // for root node
            //     bool align = true;
            //     for (int i = 0; i < PROCESSOR_NUM - 1; i++) {
            //         if (p_temp_rx[i] != p_temp_rx[i+1]) align = false;
            //     }
            //     if (!align) {
            //         std::cout << "[INFO] ASYNC occur in nodes" << std::endl;
            //         for (int i = 0; i < PROCESSOR_NUM; i++) {
            //             std::cout << "Node[" << i << "] 's p_temp_rx is " << p_temp_rx[i] << std::endl;
            //         }
            //     }
            // }
        }
    }

    // int result = APPLY_Execution_start (world_rank, world_size, &graphDataInfo, &thunderGraph);

    // result = APPLY_Execution_end (world_rank, world_size, &graphDataInfo, &thunderGraph);

    auto end = chrono::steady_clock::now();
    std::cout << "Graph kernel process elapses " << (chrono::duration_cast<chrono::microseconds>(end - start_kernel).count())/super_step<< "us" << std::endl;

    // need to add result transfer function

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
