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

#include "mpi_graph_api.h"
#include "mpi_host.h"
#include "mpi_network.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

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
        binary_file = "./acc_last_hw_sync.xclbin";
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

        MPI_Barrier(MPI_COMM_WORLD); // sync barrier;
        auto start_kernel_superstep = chrono::steady_clock::now();

        for (int p_idx = 0; p_idx < graphDataInfo.partitionNum; p_idx++) { // for each partition
            accGatherScatterExecute (s, world_rank, p_idx, &graphDataInfo, &thunderGraph); // proc_id, partition_id, graph_info, acc_info;
            // std::cout << "Sync partition [" << p_idx << "] ...";

            // int p_temp_rx[PROCESSOR_NUM];
            // for (int i = 0; i < PROCESSOR_NUM; i++) {
            //     p_temp_rx[i] = -1;
            // }
            // MPI_Gather(&(result), 1, MPI_INT, p_temp_rx, 1, MPI_INT, 0, MPI_COMM_WORLD); // * no need to sync, just like aw-barrier *
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
        auto end_GS_kernel_superstep = chrono::steady_clock::now();

        // MPI_Barrier(MPI_COMM_WORLD); // sync barrier;

        std::cout << "[INFO] GS kernel done in superstep " << s << world_rank << std::endl;

        // std::cout << "[INFO] Apply kernel start in superstep " << s << world_rank << "... ";

        // if (world_rank != (world_size - 1)) {
        //     accApplyStart (world_rank, world_size, &graphDataInfo, &thunderGraph);} // except last node

        // MPI_Barrier(MPI_COMM_WORLD); // sync barrier;

        // std::cout << " ... "  << world_rank ;

        // if (world_rank == (world_size - 1)) {
        //     accApplyStart (world_rank, world_size, &graphDataInfo, &thunderGraph);} // last node start
        
        // std::cout << world_rank << " ... done " << std::endl;

        // std::cout << world_rank << "[INFO] Apply kernel wait in superstep " << s << "... ";

        accApplyStart (world_rank, world_size, &graphDataInfo, &thunderGraph);

        // MPI_Barrier(MPI_COMM_WORLD); // sync barrier;

        accApplyEnd (world_rank, world_size, &graphDataInfo, &thunderGraph); // wait kernel done;

        // MPI_Barrier(MPI_COMM_WORLD); // sync barrier;

        auto end_kernel_superstep = chrono::steady_clock::now();
        std::cout << " ... done " << std::endl;
        std::cout << "[INFO] Processor " << world_rank << " GS elapses " ;
        std::cout << (chrono::duration_cast<chrono::microseconds>(end_GS_kernel_superstep - start_kernel_superstep).count()) << " us ";
        std::cout << " Overall elapses " ;
        std::cout << (chrono::duration_cast<chrono::microseconds>(end_kernel_superstep - start_kernel_superstep).count()) << " us" << std::endl;
    }

    auto end = chrono::steady_clock::now();
    std::cout << "[INFO] Processor " << world_rank << " average time costs " ;
    std::cout << (chrono::duration_cast<chrono::microseconds>(end - start_kernel).count())/super_step<< " us" << std::endl;

    // need to add result transfer function

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
