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

graphInfo graphDataInfo;

graphAccelerator thunderGraph;

int main(int argc, char **argv) {

    sda::utils::CmdLineParser parser;

    parser.addSwitch("--xclbin_file", "-x", "input binary file string", "");
    parser.addSwitch("--dataset", "-d", "dataset name", "HW"); // using HW dataset as default
    parser.parse(argc, argv);

    // Read settings
    std::string binary_file = parser.value("xclbin_file");
    std::string g_name = parser.value("dataset");
    std::string path_graph_dataset = "/data/yufeng/graph_dataset/";
    std::cout << "start main" << std::endl;

    // load graph
    acceleratorDataLoad(g_name, path_graph_dataset, &graphDataInfo);
    
    // init accelerator
    acceleratorInit(binary_file, &graphDataInfo, &thunderGraph); // init kernel , init buffer , map host and device memory

    // graph data pre-process
    acceleratorDataPreprocess(&graphDataInfo); // data prepare + data partition
    // acceleratorCModelDataPreprocess(&graphDataInfo); // cmodel data preprocess, for verification

    // transfer host data to FPGA side
    partitionTransfer(&graphDataInfo, &thunderGraph);

    // super step execution : set args and kernel run 

    int super_step_num = 1;
    auto start = chrono::steady_clock::now();

    for (int run_counter = 0 ; run_counter < super_step_num ; run_counter++) {
        acceleratorSuperStep(run_counter, &graphDataInfo, &thunderGraph);
        // std::cout << "super step " << run_counter << " execution done!" << std::endl;
        /* for verification */
        // acceleratorCModelSuperStep(run_counter, &graphDataInfo);
    }

    auto end = chrono::steady_clock::now();

    std::cout << "Graph elapses " << (chrono::duration_cast<chrono::microseconds>(end - start).count())/super_step_num<< "us" << std::endl; 

    // transfer FPGA data to host side
    resultTransfer(&graphDataInfo, &thunderGraph, super_step_num);

    // release memory resource
    int partition_num = graphDataInfo.partitionNum;

    for (int j = 0; j < SUB_PARTITION_NUM; j++) {
        for (int i = 0; i < partition_num; i++) {
            delete[] graphDataInfo.chunkEdgeData[i][j];
            std::cout << "edge data release finish" <<std::endl;
        }
        delete[] graphDataInfo.chunkTempData[j];
        delete[] graphDataInfo.chunkPropData[j];
        std::cout << "temp prop data release finish" <<std::endl;
    }

    delete[] graphDataInfo.chunkOutDegData;
    delete[] graphDataInfo.chunkOutRegData;
    std::cout << "outdeg and reg data release finish" <<std::endl;

    return 0;
}

