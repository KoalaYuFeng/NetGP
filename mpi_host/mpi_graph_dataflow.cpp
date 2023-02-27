#include <string>
#include <iostream>
#include <map>
#include <chrono>

// #include "host_graph_sw.h"
// #include "host_graph_scheduler.h"
#include "mpi_graph_api.h"
#include "mpi_host.h"
#include "mpi_network.h"


// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define USE_APPLY true
#define ROOT_NODE true // assume only GAS worker have apply kernel

// network configuration : 0 for GAS worker, other GA worker.
std::map<int, std::map<std::string, std::string>> FPGA_config = \
   {{0 , {{"ip_addr" , "192.168.0.201"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "201"}, {"MAC_addr" , "00:0a:35:02:9d:c9"}}}, \
    {1 , {{"ip_addr" , "192.168.0.202"}, {"tx_port" , "62177"}, {"rx_port" , "5001"}, {"idx" , "202"}, {"MAC_addr" , "00:0a:35:02:9d:ca"}}}, \
    {2 , {{"ip_addr" , "192.168.0.203"}, {"tx_port" , "61452"}, {"rx_port" , "5001"}, {"idx" , "203"}, {"MAC_addr" , "00:0a:35:02:9d:cb"}}}, \
    {3 , {{"ip_addr" , "192.168.0.204"}, {"tx_port" , "60523"}, {"rx_port" , "5001"}, {"idx" , "204"}, {"MAC_addr" , "00:0a:35:02:9d:cc"}}}};


int acceleratorInit(int world_rank, int world_size, std::string& file_name,  graphInfo *info, graphAccelerator* acc)
{

    // load xclbin file
    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(file_name);

    std::cout << "[INFO] Processor " << world_rank << " : Xclbin load done" <<std::endl;

    //check linkstatus
    bool linkStatus = false;
    AlveoVnxCmac cmac_0 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 0);
    AlveoVnxCmac cmac_1 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 1);
    linkStatus = (cmac_0.readRegister("stat_rx_status") & 0x1) && (cmac_1.readRegister("stat_rx_status") & 0x1);
    while(!linkStatus) {
        // std::cout << " world rank " << world_rank <<  " LinkStatus is " << linkStatus <<std::endl;
        sleep(1);
        linkStatus = (cmac_0.readRegister("stat_rx_status") & 0x1) && (cmac_1.readRegister("stat_rx_status") & 0x1);
        std::cout << "[INFO] Processor " << world_rank << " check linkstatus ... " <<std::endl;
    }
    std::cout << "[INFO] Processor " << world_rank << " check linkstatus ...  done " <<std::endl;


    // set socket table, run arp discovery;
    int upper_worker_idx = (world_rank == 0)? (world_size - 1) : (world_rank - 1);
    int lower_worker_idx = (world_rank == (world_size - 1))? 0 : (world_rank + 1);

    AlveoVnxNetworkLayer netlayer_0 = AlveoVnxNetworkLayer(acc->graphDevice, acc->graphUuid, 0);
    AlveoVnxNetworkLayer netlayer_1 = AlveoVnxNetworkLayer(acc->graphDevice, acc->graphUuid, 1);

    netlayer_0.setAddress(FPGA_config[world_rank]["ip_addr"], FPGA_config[world_rank]["MAC_addr"]);
    netlayer_0.setSocket(FPGA_config[lower_worker_idx]["ip_addr"], stoi(FPGA_config[lower_worker_idx]["tx_port"]), 5001, 0); // set recv socket
    netlayer_0.setSocket(FPGA_config[lower_worker_idx]["ip_addr"], 5001, stoi(FPGA_config[world_rank]["tx_port"]), 1); // set send socket
    netlayer_0.getSocketTable();

    netlayer_1.setAddress(FPGA_config[world_rank]["ip_addr"], FPGA_config[world_rank]["MAC_addr"]);
    netlayer_1.setSocket(FPGA_config[upper_worker_idx]["ip_addr"], stoi(FPGA_config[upper_worker_idx]["tx_port"]), 5001, 0); // set recv socket
    netlayer_1.setSocket(FPGA_config[upper_worker_idx]["ip_addr"], 5001, stoi(FPGA_config[world_rank]["tx_port"]), 1); // set send socket
    netlayer_1.getSocketTable();

    bool ARP_ready = false;
    while(!ARP_ready) {
        netlayer_0.runARPDiscovery();
        netlayer_1.runARPDiscovery();
        usleep(500000);
        ARP_ready = (netlayer_0.IsARPTableFound(FPGA_config[lower_worker_idx]["ip_addr"])) \
                    && (netlayer_1.IsARPTableFound(FPGA_config[upper_worker_idx]["ip_addr"]));
        std::cout << "[INFO] Processor " << world_rank << " wait ARP ... " << std::endl;
    }
    std::cout << "[INFO] Processor " << world_rank << " wait ARP ... done " << std::endl;


    // init FPGA kernels
    std::string id, krnl_name;
    for (int i = 0; i < (SUB_PARTITION_NUM / PROCESSOR_NUM); i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "readEdgesCU1:{readEdgesCU1_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

#if USE_APPLY

    for (int i = 0; i < (SUB_PARTITION_NUM / PROCESSOR_NUM); i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "streamMerge:{streamMerge_" + id + "}";
        acc->mergeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "streamForward:{streamForward_" + id + "}";
        acc->forwardKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "readVertex:{readVertex_" + id + "}";
        acc->readKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "writeVertex:{writeVertex_" + id + "}";
        acc->writeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

    if (world_rank == 0)  // GAS worker, additional apply kernel and sync kernel
    {
        krnl_name = "vertexApply:{vertexApply_1}";
        acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "syncVertex:{syncVertex_1}";
        acc->syncKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

#endif

    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;
    long unsigned int temp_prop_size = 0;

    acc->edgeBuffer.resize(info->partitionNum);
    acc->tempBuffer.resize(info->partitionNum);

    for (int i = 0; i < info->partitionNum; i++) {
        acc->edgeBuffer[i].resize((SUB_PARTITION_NUM / PROCESSOR_NUM));
        acc->tempBuffer[i].resize((SUB_PARTITION_NUM / PROCESSOR_NUM));

        for (int j = 0; j < (SUB_PARTITION_NUM / PROCESSOR_NUM); j++) {
            int proc_j = j * PROCESSOR_NUM + world_rank;
            edge_size_bytes = info->chunkProp[i][proc_j].edgeNumChunk * 2 * sizeof(int); // each edge has two vertex, vertex index uses int type;
            acc->edgeBuffer[i][j] = xrt::bo(acc->graphDevice, edge_size_bytes, acc->gsKernel[j].group_id(0));
            info->chunkEdgeData[i][proc_j] = acc->edgeBuffer[i][j].map<int*>();

            prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex prop;
            acc->propBuffer[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->gsKernel[j].group_id(1));
            info->chunkPropData[proc_j] = acc->propBuffer[j].map<int*>();

            temp_prop_size = PARTITION_SIZE * sizeof(int); // temp vertex size. // need improvement
            acc->tempBuffer[i][j] = xrt::bo(acc->graphDevice, temp_prop_size, acc->gsKernel[j].group_id(1));
            info->chunkTempData[i][proc_j] = acc->tempBuffer[i][j].map<int*>();
        }

#if USE_APPLY

        for (int j = 0; j < (SUB_PARTITION_NUM / PROCESSOR_NUM); j++) {
            int proc_j = j * PROCESSOR_NUM + world_rank;
            prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // new vertex prop;
            acc->propBufferNew[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->writeKernel[j].group_id(0));
            info->chunkPropDataNew[proc_j] = acc->propBufferNew[j].map<int*>();
        }

        acc->subNewBuffer.resize(info->partitionNum);
        acc->subBuffer.resize(info->partitionNum);
        for (int p = 0; p < info->partitionNum; p++) {
            acc->subNewBuffer[p].resize(SUB_PARTITION_NUM / PROCESSOR_NUM);
            acc->subBuffer[p].resize(SUB_PARTITION_NUM / PROCESSOR_NUM);
            for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
                // int proc_sp = sp * PROCESSOR_NUM + world_rank;
                int size_tmp = PARTITION_SIZE * sizeof(int);
                int offset_tmp = p * PARTITION_SIZE * sizeof(int);
                acc->subNewBuffer[p][sp] = xrt::bo(acc->propBufferNew[sp], size_tmp, offset_tmp); // size, offset
                acc->subBuffer[p][sp] = xrt::bo(acc->propBuffer[sp], size_tmp, offset_tmp);
            }
        }

        if (world_rank == 0) { // GAS worker
            outDeg_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex number * sizeof(int)
            acc->propApplyBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));
            acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));
            acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));
            info->chunkApplyPropData = acc->propApplyBuffer.map<int*>();
            info->chunkOutDegData = acc->outDegBuffer.map<int*>();
            info->chunkOutRegData = acc->outRegBuffer.map<int*>();
        }

#endif

    }

    std::cout << "[INFO] Accelerator init done ! " << std::endl;
    return 0;
}

int accGatherScatterExecute (int super_step, int world_rank, int partition, graphInfo *info, graphAccelerator * acc) {

    std::cout << "[INFO] GS " << super_step << " Execute at processor " << world_rank << " partition " << partition << "..." << std::endl;

    if (partition < 0) return -1; // check the right parition id;

    int p = partition;
    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        // auto start = std::chrono::steady_clock::now();
        int isp = world_rank + sp * PROCESSOR_NUM;
        acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
        acc->gsRun[sp].set_arg(1, acc->propBuffer[sp]);
        acc->gsRun[sp].set_arg(2, acc->tempBuffer[p][sp]);
        acc->gsRun[sp].set_arg(3, info->chunkProp[p][isp].edgeNumChunk * 2);
        acc->gsRun[sp].start();

        // acc->gsRun[sp].wait();
        // auto end = std::chrono::steady_clock::now();
        // std::cout << "[Schedule] world_rank " << world_rank << " sp = " << sp << " cost = "
        // << (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) << " us " << std::endl;
    }

    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        acc->gsRun[sp].wait();
    }

    return partition;
}

int accApplyStart (int world_rank, int world_size, graphInfo *info, graphAccelerator * acc) {

    if (world_rank == 0) { // GAS worker
        // Apply kernel start

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].set_arg(0, acc->propBuffer[sp]);
            acc->readRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->mergeRun[sp].set_arg(3, 1);
            acc->mergeRun[sp].start();
        }

        acc->applyRun.set_arg(0, acc->propApplyBuffer);
        acc->applyRun.start();

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->forwardRun[sp].set_arg(3, 1); // dest = 1;
            acc->forwardRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].set_arg(0, acc->propBufferNew[sp]);
            acc->writeRun[sp].start();
        }

        acc->syncRun.start();

    } else { // GS worker

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].set_arg(0, acc->propBufferNew[sp]);
            acc->writeRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->forwardRun[sp].set_arg(3, 1); // dest = 0;
            acc->forwardRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].set_arg(0, acc->propBuffer[sp]);
            acc->readRun[sp].start();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->mergeRun[sp].set_arg(3, 1); // dest = 1;
            acc->mergeRun[sp].start();
        }
    }
    return 0;
}

int accApplyEnd (int world_rank, int world_size, graphInfo *info, graphAccelerator * acc) {

    if (world_rank == 0) { // GAS worker

        acc->syncRun.wait();

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].wait();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->mergeRun[sp].wait();
        }

        acc->applyRun.wait();

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->forwardRun[sp].wait();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].wait();
        }

    } else { // GS worker

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->readRun[sp].wait();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->mergeRun[sp].wait();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->forwardRun[sp].wait();
        }

        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->writeRun[sp].wait();
        }

    }

    return 0;

}


void partitionTransfer(int world_rank, graphInfo *info, graphAccelerator * acc)
{
    // Synchronize buffer content with device side

#if USE_APPLY
    if (world_rank == 0) { // GAS worker
        acc->outDegBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        acc->outRegBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        acc->propApplyBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

#endif

    for (int i = 0; i < info->partitionNum; i++) {
        for (int j = 0; j < (SUB_PARTITION_NUM / PROCESSOR_NUM); j++) {
            acc->propBuffer[j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
            acc->edgeBuffer[i][j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }
}

void setAccKernelArgs(int world_rank, int world_size, graphInfo *info, graphAccelerator * acc)
{
    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
            acc->gsRun[sp] = xrt::run(acc->gsKernel[sp]);
            acc->gsRun[sp].set_arg(4, 0);
            acc->gsRun[sp].set_arg(5, info->alignedCompressedVertexNum);
        }
    }

#if USE_APPLY

    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        acc->readRun[sp] = xrt::run(acc->readKernel[sp]);
        acc->readRun[sp].set_arg(2, info->alignedCompressedVertexNum);
        acc->mergeRun[sp] = xrt::run(acc->mergeKernel[sp]);
        acc->mergeRun[sp].set_arg(4, info->alignedCompressedVertexNum);
        acc->forwardRun[sp] = xrt::run(acc->forwardKernel[sp]);
        acc->forwardRun[sp].set_arg(4, info->alignedCompressedVertexNum);
        acc->writeRun[sp] = xrt::run(acc->writeKernel[sp]);
        acc->writeRun[sp].set_arg(2, info->alignedCompressedVertexNum);
    }

    if (world_rank == 0) { // GAS worker
        acc->applyRun = xrt::run(acc->applyKernel);
        // acc->applyRun.set_arg(0, acc->propBuffer[0]);
        acc->applyRun.set_arg(3, acc->outDegBuffer);
        acc->applyRun.set_arg(4, acc->outRegBuffer);
        acc->applyRun.set_arg(5, info->alignedCompressedVertexNum); // depends on which SLR
        acc->applyRun.set_arg(6, 0);
        acc->applyRun.set_arg(7, 0);
        acc->syncRun = xrt::run(acc->syncKernel);
        acc->syncRun.set_arg(2, 1);
        acc->syncRun.set_arg(3, 2048); // FIFO length
        acc->syncRun.set_arg(4, info->alignedCompressedVertexNum); // vertex number
    }

#endif

}

void resultTransfer(graphInfo *info, graphAccelerator * acc, int run_counter)
{
    // Transfer device buffer content to host side
    acc->outRegBuffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    for (int sp = 0; sp < (SUB_PARTITION_NUM / PROCESSOR_NUM); sp++) {
        if (run_counter % 2 == 0) {
            acc->propBufferNew[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        } else {
            acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }

        // for (int size = 0; size < info->chunkProp[p][sp].destVertexNumChunk; size++) {
        //     std::cout << "[" << p <<"]["<< sp << "]["<< size << "]:" << info->chunkTempData[p][sp][size];
        //     if ((size + 1) % 5 == 0) std::cout << std::endl;
        // }

        // std::cout << std::endl;
    }
    std::cout << "[INFO] Sync data (temp, outReg) to host " << std::endl;

}

