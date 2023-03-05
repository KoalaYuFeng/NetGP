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

bool sorting (int i,int j) { return (i<j); } // incremental order.

int main(int argc, char** argv) {

    sda::utils::CmdLineParser parser;
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.parse(argc, argv);

    log_set_level(LOG_INFO); // set log level LOG_INFO;

    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    log_debug("MPI init done: %d / %d ", world_rank, world_size);

    if (world_size != PROCESSOR_NUM) {
        log_error("MPI number does not equal to setting !");
        return 0;
    }

    graphInfo graphDataInfo;
    graphAccelerator thunderGraph;
    std::string g_name = parser.value("dataset");

    log_info("[INFO] Processor %d start ... loading graph %s", world_rank, g_name.c_str());

    acceleratorDataLoad(g_name, &graphDataInfo);
    log_info("[INFO] Processor %d graph load done", world_rank);

    acceleratorInit(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor %d acc init done", world_rank);

    acceleratorDataPreprocess(g_name, &graphDataInfo); // data prepare + data partition
    log_info("[INFO] Processor %d graph preprocess done", world_rank);
    // acceleratorCModelDataPreprocess(&graphDataInfo); // cmodel data preprocess, for verification

    partitionTransfer(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor %d partitionTransfer done", world_rank);

    setAccKernelArgs(&graphDataInfo, &thunderGraph);
    log_info("[INFO] Processor %d setAccKernelArgs done", world_rank);

    MPI_Barrier(MPI_COMM_WORLD); // sync barrier;


    // run super steps
    int super_step = SUPERSTEP_NUM; // run 10 super steps.
    std::vector<int> time_array;
    time_array.resize(super_step);

    for (int s = 0; s < super_step; s++) {

        auto start_kernel_superstep = chrono::steady_clock::now();

        acceleratorExecute (s, &graphDataInfo, &thunderGraph);

        auto end_kernel_superstep = chrono::steady_clock::now();
        time_array[s] = chrono::duration_cast<chrono::microseconds>(end_kernel_superstep - start_kernel_superstep).count();

    }

    std::sort(time_array.begin(), time_array.end(), sorting);
    int average_t = 0;
    for (std::vector<int>::iterator it=time_array.begin(); it!=time_array.end(); ++it) {
        average_t += (*it);
    }
    average_t = average_t / super_step;

    log_info("[INFO] Processor %d, time cost: min %d us, middle %d us, max %d us, average %d us.", \
            world_rank, time_array[0], time_array[super_step/2], time_array[super_step-1], average_t);

    // need to add result transfer function

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
