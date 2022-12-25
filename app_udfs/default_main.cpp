#include "cmdlineparser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <chrono>

#include "host_graph_api.h"
#include "host_graph_verification.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

using namespace std;

int main(int argc, char **argv) {

    graphInfo graphDataInfo;
    graphAccelerator thunderGraph;

    sda::utils::CmdLineParser parser;

    parser.addSwitch("--xclbin_file", "-x", "input binary file string", "");
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.parse(argc, argv);

    // Read settings
    std::string binary_file = parser.value("xclbin_file");
    std::string g_name = parser.value("dataset");
    std::string path_graph_dataset = "/data/binary_graph_dataset/";
    std::cout << "[INFO] Start main" << std::endl;

    auto start_main = chrono::steady_clock::now();

    // load graph
    acceleratorDataLoad(g_name, path_graph_dataset, &graphDataInfo);

    // init accelerator
    acceleratorInit(binary_file, &graphDataInfo, &thunderGraph); // init kernel , init buffer , map host and device memory

    // graph data pre-process
    acceleratorDataPreprocess(&graphDataInfo); // data prepare + data partition
    // acceleratorCModelDataPreprocess(&graphDataInfo); // cmodel data preprocess, for verification

    // transfer host data to FPGA side
    partitionTransfer(&graphDataInfo, &thunderGraph);

    // set acclerator kernel args.
    setAccKernelArgs(&graphDataInfo, &thunderGraph);

    // super step execution : set args and kernel run 

    int super_step_num = 100;
    auto start_kernel = chrono::steady_clock::now();

    for (int run_counter = 0 ; run_counter < super_step_num ; run_counter++) {
        acceleratorSuperStep(run_counter, &graphDataInfo, &thunderGraph);
        // std::cout << "super step " << run_counter << " execution done!" << std::endl;
        /* for verification */
        // acceleratorCModelSuperStep(run_counter, &graphDataInfo);
    }

    auto end = chrono::steady_clock::now();

    std::cout << "Graph preprocess[load + partition] elapses " << (chrono::duration_cast<chrono::seconds>(start_kernel - start_main).count())<< "s" << std::endl; 

    std::cout << "Graph kernel process elapses " << (chrono::duration_cast<chrono::microseconds>(end - start_kernel).count())/super_step_num<< "us" << std::endl; 

    // transfer FPGA data to host side
    resultTransfer(&graphDataInfo, &thunderGraph, super_step_num);

    // release memory resource
    // for (int p = 0; p < graphDataInfo.partitionNum; p++) {
    //     for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //         delete [] graphDataInfo.chunkEdgeData[p][sp];
    //         delete [] graphDataInfo.chunkTempData[p][sp];
    //     }
    // }
    // std::cout << "[INFO] Release Edge and Temp data finish" <<std::endl;

    // for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
    //     delete [] graphDataInfo.chunkPropDataNew[sp];
    //     delete [] graphDataInfo.chunkPropData[sp];
    // }
    // std::cout << "[INFO] Release Prop data finish" <<std::endl;

    // delete [] graphDataInfo.chunkOutDegData;
    // delete [] graphDataInfo.chunkOutRegData;
    // std::cout << "[INFO] Release Outdeg and Outreg data finish" <<std::endl;

    return 0;
}

