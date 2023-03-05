#include <string>
#include <iostream>
#include <map>
#include <mpi.h>
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

// network configuration : 0 for GAS worker, other GA worker.
std::map<int, std::map<std::string, std::string>> FPGA_config = \
   {{0 , {{"ip_addr" , "192.168.0.201"}, {"tx_port" , "60512"}, {"rx_port" , "5001"}, {"idx" , "201"}, {"MAC_addr" , "00:0a:35:02:9d:c9"}}}, \
    {1 , {{"ip_addr" , "192.168.0.202"}, {"tx_port" , "62177"}, {"rx_port" , "5001"}, {"idx" , "202"}, {"MAC_addr" , "00:0a:35:02:9d:ca"}}}, \
    {2 , {{"ip_addr" , "192.168.0.203"}, {"tx_port" , "61452"}, {"rx_port" , "5001"}, {"idx" , "203"}, {"MAC_addr" , "00:0a:35:02:9d:cb"}}}, \
    {3 , {{"ip_addr" , "192.168.0.204"}, {"tx_port" , "60523"}, {"rx_port" , "5001"}, {"idx" , "204"}, {"MAC_addr" , "00:0a:35:02:9d:cc"}}}};


int acceleratorInit(graphInfo *info, graphAccelerator* acc)
{
    // load xclbin file
    int world_size = -1;
    int world_rank = -1;
    std::string xclbin_file;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    if (world_size < 0) {
        log_error("world size error! ");
        return 0;
    } else if (world_size == 1) {
        xclbin_file = "./xclbin_single/GAS_worker.xclbin"; // single node;
    } else { // multi node
        if (world_rank == 0) {
            xclbin_file = "./xclbin_3SLRs_overlap/GAS_worker.xclbin"; // GAS worker
        } else {
            xclbin_file = "./xclbin_3SLRs_overlap/GS_worker.xclbin"; // GS worker
        }
    }

    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(xclbin_file);

    log_debug("[INFO] Processor %d : Xclbin load done ", world_rank);

    //check linkstatus. Only for multi node case. Single node skip this.
    if (PROCESSOR_NUM > 1) {
        bool linkStatus = false;
        AlveoVnxCmac cmac_0 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 0);
        AlveoVnxCmac cmac_1 = AlveoVnxCmac(acc->graphDevice, acc->graphUuid, 1);
        linkStatus = (cmac_0.readRegister("stat_rx_status") & 0x1) && (cmac_1.readRegister("stat_rx_status") & 0x1);
        while(!linkStatus) {
            sleep(1);
            linkStatus = (cmac_0.readRegister("stat_rx_status") & 0x1) && (cmac_1.readRegister("stat_rx_status") & 0x1);
            log_debug("[INFO] Processor %d, check linkstatus ...", world_rank);
        }
        log_debug("[INFO] Processor %d, check linkstatus ...  done", world_rank);


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
            log_debug("[INFO] Processor %d, wait ARP ... ", world_rank);
        }
        log_debug("[INFO] Processor %d, wait ARP ... done ", world_rank);
    }
    log_trace("network connect, world rank %d", world_rank);


    // init FPGA kernels
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;
    std::string id, krnl_name;
    for (int i = 0; i < kernel_per_node; i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "readEdgesCU1:{readEdgesCU1_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

#if USE_APPLY

    for (int i = 0; i < kernel_per_node - 1; i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "streamMerge:{streamMerge_" + id + "}";
        acc->mergeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "streamForward:{streamForward_" + id + "}";
        acc->forwardKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        krnl_name = "writeVertex:{writeVertex_" + id + "}";
        acc->writeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

#if (PROCESSOR_NUM != 1)
    // in single node, the number of merge and forward should be (kernel_per_node - 1).
    krnl_name = "streamMerge:{streamMerge_" + std::to_string(kernel_per_node) + "}";
    acc->mergeKernel[kernel_per_node - 1] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    krnl_name = "streamForward:{streamForward_" + std::to_string(kernel_per_node) + "}";
    acc->forwardKernel[kernel_per_node - 1] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
#endif
    krnl_name = "writeVertex:{writeVertex_" + std::to_string(kernel_per_node) + "}";
    acc->writeKernel[kernel_per_node - 1] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());

    if (world_rank == 0)  // GAS worker, additional apply kernel and sync kernel
    {
        krnl_name = "vertexApply:{vertexApply_1}";
        acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
#if (PROCESSOR_NUM != 1)
        krnl_name = "syncVertex:{syncVertex_1}";
        acc->syncKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
#endif
    }

#endif
    log_trace("kernel init done, world rank %d", world_rank);

    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;

    acc->edgeBuffer.resize(info->partitionNum);
    for (int i = 0; i < info->partitionNum; i++) {
        acc->edgeBuffer[i].resize(kernel_per_node);
        for (int j = 0; j < kernel_per_node; j++) {
            edge_size_bytes = info->chunkProp[i][j].edgeNumChunk * 2 * sizeof(int); // each edge has two vertex, vertex index uses int type;
            acc->edgeBuffer[i][j] = xrt::bo(acc->graphDevice, edge_size_bytes, acc->gsKernel[j].group_id(0));
            info->chunkEdgeData[i][j] = acc->edgeBuffer[i][j].map<int*>();

            prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex prop;
            acc->propBuffer[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->gsKernel[j].group_id(1));
            info->chunkPropData[j] = acc->propBuffer[j].map<int*>();
        }

#if USE_APPLY

        for (int j = 0; j < kernel_per_node; j++) {
            prop_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // new vertex prop;
            acc->propBufferNew[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->writeKernel[j].group_id(0));
            info->chunkPropDataNew[j] = acc->propBufferNew[j].map<int*>();
        }

        acc->subNewBuffer.resize(info->partitionNum);
        acc->subBuffer.resize(info->partitionNum);
        for (int p = 0; p < info->partitionNum; p++) {
            acc->subNewBuffer[p].resize(kernel_per_node);
            acc->subBuffer[p].resize(kernel_per_node);
            for (int sp = 0; sp < kernel_per_node; sp++) {
                int size_tmp = PARTITION_SIZE * sizeof(int);
                int offset_tmp = p * PARTITION_SIZE * sizeof(int);
                acc->subNewBuffer[p][sp] = xrt::bo(acc->propBufferNew[sp], size_tmp, offset_tmp); // size, offset
                acc->subBuffer[p][sp] = xrt::bo(acc->propBuffer[sp], size_tmp, offset_tmp);
            }
        }

        if (world_rank == 0) { // GAS worker
            outDeg_size_bytes = info->alignedCompressedVertexNum * sizeof(int); // vertex number * sizeof(int)
            acc->propApplyBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));
            acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(3));
            acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(4));
            info->chunkApplyPropData = acc->propApplyBuffer.map<int*>();
            info->chunkOutDegData = acc->outDegBuffer.map<int*>();
            info->chunkOutRegData = acc->outRegBuffer.map<int*>();
        }

#endif

    }

    log_trace("kernel map done, world rank %d", world_rank);
    return 0;
}

int acceleratorExecute (int super_step, graphInfo *info, graphAccelerator * acc) {
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;

    log_debug("[INFO] SuperStep %d Execute at processor %d ", super_step , world_rank);

    for (int p = 0; p < info->partitionNum; p++) {
        if (world_rank == 0) {
            acc->applyRun.set_arg(0, acc->propApplyBuffer);
            acc->applyRun.start();
#if (PROCESSOR_NUM != 1)
            acc->syncRun.start();
#endif
        }

        for (int sp = 0; sp < (kernel_per_node - 1); sp++) {
            // auto start = std::chrono::steady_clock::now();
            acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
            acc->gsRun[sp].set_arg(1, acc->propBuffer[sp]);
            acc->gsRun[sp].set_arg(3, info->chunkProp[p][sp].edgeNumChunk * 2);
            acc->gsRun[sp].start();
            acc->mergeRun[sp].set_arg(3, 1);
            acc->mergeRun[sp].start();
            acc->forwardRun[sp].set_arg(3, 1); // dest = 1;
            acc->forwardRun[sp].start();
            acc->writeRun[sp].set_arg(0, acc->subNewBuffer[p][sp]);
            acc->writeRun[sp].start();
        }
        acc->gsRun[kernel_per_node - 1].set_arg(0, acc->edgeBuffer[p][kernel_per_node - 1]);
        acc->gsRun[kernel_per_node - 1].set_arg(1, acc->propBuffer[kernel_per_node - 1]);
        acc->gsRun[kernel_per_node - 1].set_arg(3, info->chunkProp[p][kernel_per_node - 1].edgeNumChunk * 2);
        acc->gsRun[kernel_per_node - 1].start();
#if (PROCESSOR_NUM != 1)
        acc->mergeRun[kernel_per_node - 1].set_arg(3, 1);
        acc->mergeRun[kernel_per_node - 1].start();
        acc->forwardRun[kernel_per_node - 1].set_arg(3, 1); // dest = 1;
        acc->forwardRun[kernel_per_node - 1].start();
#endif
        acc->writeRun[kernel_per_node - 1].set_arg(0, acc->subNewBuffer[p][kernel_per_node - 1]);
        acc->writeRun[kernel_per_node - 1].start();

        for (int sp = 0; sp < (kernel_per_node - 1); sp++) {
            acc->gsRun[sp].wait();
            acc->mergeRun[sp].wait();
            acc->forwardRun[sp].wait();
            acc->writeRun[sp].wait();
        }
        acc->gsRun[kernel_per_node - 1].wait();
#if (PROCESSOR_NUM != 1)
        acc->mergeRun[kernel_per_node - 1].wait();
        acc->forwardRun[kernel_per_node - 1].wait();
#endif
        acc->writeRun[kernel_per_node - 1].wait();

        if (world_rank == 0) {
            acc->applyRun.wait();
#if (PROCESSOR_NUM != 1)
            acc->syncRun.wait();
#endif
        }
    }

    return 0;
}


void partitionTransfer(graphInfo *info, graphAccelerator * acc)
{
    // Synchronize buffer content with device side
    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank < 0) log_error("world rank error !");

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

void setAccKernelArgs(graphInfo *info, graphAccelerator * acc)
{

    int world_rank = -1;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    if (world_rank < 0) log_error("world rank error !");
    int kernel_per_node = KERNEL_NUM / PROCESSOR_NUM;


    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < kernel_per_node; sp++) {
            acc->gsRun[sp] = xrt::run(acc->gsKernel[sp]);
            acc->gsRun[sp].set_arg(4, 0);
            acc->gsRun[sp].set_arg(5, PARTITION_SIZE); // should be dest vertex number, but I align, need modify
        }
    }

#if USE_APPLY

    for (int sp = 0; sp < kernel_per_node - 1; sp++) {
        acc->mergeRun[sp] = xrt::run(acc->mergeKernel[sp]);
        acc->mergeRun[sp].set_arg(4, PARTITION_SIZE);
        acc->forwardRun[sp] = xrt::run(acc->forwardKernel[sp]);
        acc->forwardRun[sp].set_arg(4, PARTITION_SIZE);
        acc->writeRun[sp] = xrt::run(acc->writeKernel[sp]);
        acc->writeRun[sp].set_arg(2, PARTITION_SIZE);
    }
#if (PROCESSOR_NUM != 1)
    acc->mergeRun[kernel_per_node - 1] = xrt::run(acc->mergeKernel[kernel_per_node - 1]);
    acc->mergeRun[kernel_per_node - 1].set_arg(4, PARTITION_SIZE);
    acc->forwardRun[kernel_per_node - 1] = xrt::run(acc->forwardKernel[kernel_per_node - 1]);
    acc->forwardRun[kernel_per_node - 1].set_arg(4, PARTITION_SIZE);
#endif
    acc->writeRun[kernel_per_node - 1] = xrt::run(acc->writeKernel[kernel_per_node - 1]);
    acc->writeRun[kernel_per_node - 1].set_arg(2, PARTITION_SIZE);

    if (world_rank == 0) { // GAS worker
        acc->applyRun = xrt::run(acc->applyKernel);
        // acc->applyRun.set_arg(0, acc->propBuffer[0]);
        acc->applyRun.set_arg(3, acc->outDegBuffer);
        acc->applyRun.set_arg(4, acc->outRegBuffer);
        // acc->applyRun.set_arg(5, info->alignedCompressedVertexNum); // depends on which SLR
        acc->applyRun.set_arg(5, PARTITION_SIZE); // depends on which SLR
        acc->applyRun.set_arg(6, 0);
        acc->applyRun.set_arg(7, 0);
#if (PROCESSOR_NUM != 1)
        acc->syncRun = xrt::run(acc->syncKernel);
        acc->syncRun.set_arg(2, 1);
        acc->syncRun.set_arg(3, 2048); // FIFO length
        acc->syncRun.set_arg(4, PARTITION_SIZE); // vertex number
#endif
    }

#endif

}

/*
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
    }
    log_trace("[INFO] Sync data (temp, outReg) to host ");

}
*/

