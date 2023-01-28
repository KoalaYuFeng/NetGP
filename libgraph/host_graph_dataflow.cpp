
#include "host_graph_sw.h"
#include "host_graph_scheduler.h"
#include "host_graph_api.h"
#include <cstring>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define USE_APPLY true

int acceleratorInit(std::string& file_name,  graphInfo *info, graphAccelerator* acc)
{

    // load xclbin file
    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(file_name);

    std::cout << "[INFO] Xclbin load done" <<std::endl;

    // init FPGA kernels
    std::string id, krnl_name;
    for (int i = 0; i < SUB_PARTITION_NUM; i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "readEdgesCU1:{readEdgesCU1_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }

#if USE_APPLY

    for (int i = 0; i < SUB_PARTITION_NUM - 1; i++)
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

    krnl_name = "readVertex:{readVertex_" + std::to_string(SUB_PARTITION_NUM) + "}";
    acc->readKernel[SUB_PARTITION_NUM - 1] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    krnl_name = "writeVertex:{writeVertex_" + std::to_string(SUB_PARTITION_NUM) + "}";
    acc->writeKernel[SUB_PARTITION_NUM - 1] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());

    std::string krnl_name_full = "vertexApply:{vertexApply_1}";
    acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name_full.c_str());

#endif

    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;
    long unsigned int temp_prop_size = 0;

    acc->edgeBuffer.resize(info->partitionNum);
    acc->tempBuffer.resize(info->partitionNum);

    for (int i = 0; i < info->partitionNum; i++) {
        acc->edgeBuffer[i].resize(SUB_PARTITION_NUM);
        acc->tempBuffer[i].resize(SUB_PARTITION_NUM);

        for (int j = 0; j < SUB_PARTITION_NUM; j++) {
            edge_size_bytes = info->chunkProp[i][j].edgeNumChunk * 2 * sizeof(int); // each edge has two vertex, vertex index uses int type;
            acc->edgeBuffer[i][j] = xrt::bo(acc->graphDevice, edge_size_bytes, acc->gsKernel[j].group_id(0));
            info->chunkEdgeData[i][j] = acc->edgeBuffer[i][j].map<int*>();

            prop_size_bytes = info->partitionNum * PARTITION_SIZE * sizeof(prop_t); // vertex prop;
            acc->propBuffer[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->gsKernel[j].group_id(1));
            info->chunkPropData[j] = acc->propBuffer[j].map<int*>();

            temp_prop_size = info->chunkProp[i][j].destVertexNumChunk * sizeof(prop_t); // temp vertex size
            acc->tempBuffer[i][j] = xrt::bo(acc->graphDevice, temp_prop_size, acc->gsKernel[j].group_id(1));
            info->chunkTempData[i][j] = acc->tempBuffer[i][j].map<int*>();
        }

#if USE_APPLY

        for (int j = 0; j < SUB_PARTITION_NUM; j++) {
            prop_size_bytes = info->partitionNum * PARTITION_SIZE * sizeof(prop_t); // new vertex prop;
            acc->propBufferNew[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->writeKernel[j].group_id(0));
            info->chunkPropDataNew[j] = acc->propBufferNew[j].map<int*>();
        }

        acc->subNewBuffer.resize(info->partitionNum);
        acc->subBuffer.resize(info->partitionNum);
        for (int p = 0; p < info->partitionNum; p++) {
            acc->subNewBuffer[p].resize(SUB_PARTITION_NUM);
            acc->subBuffer[p].resize(SUB_PARTITION_NUM);
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                int size_tmp = info->chunkProp[p][sp].destVertexNumChunk * sizeof(prop_t);
                int offset_tmp = p * PARTITION_SIZE * sizeof(prop_t);
                acc->subNewBuffer[p][sp] = xrt::bo(acc->propBufferNew[sp], size_tmp, offset_tmp); // size, offset
                acc->subBuffer[p][sp] = xrt::bo(acc->propBuffer[sp], size_tmp, offset_tmp);
            }
        }

        outDeg_size_bytes = info->alignedCompressedVertexNum * sizeof(prop_t); // vertex number * sizeof(int)
        acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));
        acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));
        acc->propApplyBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));

        info->chunkOutDegData = acc->outDegBuffer.map<int*>();
        info->chunkOutRegData = acc->outRegBuffer.map<int*>();
        info->chunkApplyPropData = acc->propApplyBuffer.map<int*>();

#endif

    }

    std::cout << "[INFO] Accelerator init done ! " << std::endl;
    return 0;
}

int acceleratorSuperStep(int superStep, graphInfo *info, graphAccelerator * acc)
{
    for (int p = 0; p < info->partitionNum + 2; p++) {

        if (p == 0) { // first partition, only start GS kernel;
            // GathS kernel start
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                if (superStep % 2 == 0) {
                    acc->gsRun[sp].set_arg(1, acc->propBuffer[sp]);
                } else {
                    acc->gsRun[sp].set_arg(1, acc->propBufferNew[sp]);
                }
                acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
                acc->gsRun[sp].set_arg(2, acc->tempBuffer[p][sp]);
                acc->gsRun[sp].set_arg(3, info->chunkProp[p][sp].edgeNumChunk * 2);
                acc->gsRun[sp].start();
            }

        } else if (p < info->partitionNum + 1) {
            // GS kernel wait
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                acc->gsRun[sp].wait(); }
            
            if (p < info->partitionNum) {
                // GS kernel start
                for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                    if (superStep % 2 == 0) {
                        acc->gsRun[sp].set_arg(1, acc->propBuffer[sp]);
                    } else {
                        acc->gsRun[sp].set_arg(1, acc->propBufferNew[sp]);
                    }
                    acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
                    acc->gsRun[sp].set_arg(2, acc->tempBuffer[p][sp]);
                    acc->gsRun[sp].set_arg(3, info->chunkProp[p][sp].edgeNumChunk * 2);
                    acc->gsRun[sp].start();
                }
            }

            if (p > 1) {
                // Apply kernel wait
                for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                    acc->readRun[sp].wait(); }
                for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
                    acc->mergeRun[sp].wait(); }
                acc->applyRun.wait();
                for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
                    acc->forwardRun[sp].wait(); }
                for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                    acc->writeRun[sp].wait(); }
            }

            // Apply kernel start
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                acc->readRun[sp].set_arg(0, acc->tempBuffer[p-1][sp]);
                acc->readRun[sp].set_arg(2, info->chunkProp[p-1][sp].destVertexNumChunk);
                acc->readRun[sp].start();
            }

            for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
                acc->mergeRun[sp].set_arg(3, 0); // dest = 0;
                acc->mergeRun[sp].set_arg(4, info->chunkProp[p-1][sp].destVertexNumChunk);
                acc->mergeRun[sp].start();
            }

            acc->applyRun.set_arg(0, acc->propApplyBuffer);
            acc->applyRun.set_arg(5, info->chunkProp[p-1][0].destVertexNumChunk); // depends on which SLR
            acc->applyRun.start();

            for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
                acc->forwardRun[sp].set_arg(3, 1); // dest = 1;
                acc->forwardRun[sp].set_arg(4, info->chunkProp[p-1][sp].destVertexNumChunk);
                acc->forwardRun[sp].start();
            }

            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                if (superStep % 2 == 0) {
                    acc->writeRun[sp].set_arg(0, acc->subNewBuffer[p-1][sp]);
                } else {
                    acc->writeRun[sp].set_arg(0, acc->subBuffer[p-1][sp]);
                }
                acc->writeRun[sp].set_arg(2, info->chunkProp[p-1][sp].destVertexNumChunk);
                acc->writeRun[sp].start();
            }

        } else { // wait last apply kernel end
            // Apply kernel wait
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                acc->readRun[sp].wait(); }
            for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
                acc->mergeRun[sp].wait(); }
            acc->applyRun.wait();
            for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
                acc->forwardRun[sp].wait(); }
            for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
                acc->writeRun[sp].wait(); }
        }
    }
    return 0;
}


void* acceleratorQueryRegister(void)
{
    // graphAccelerator * acc = getAccelerator();
    // transfer_data_from_pl(acc->context, acc->device,MEM_ID_RESULT_REG);
    // return get_host_mem_pointer(MEM_ID_RESULT_REG);
    return 0;
}

prop_t* acceleratorQueryProperty(int step)
{
    // graphAccelerator * acc = getAccelerator();
    // transfer_data_from_pl(acc->context, acc->device, getGatherScatter(0)->prop[step].id);
    // prop_t * propValue = (prop_t *)get_host_mem_pointer(getGatherScatter(0)->prop[step].id);

    // return propValue;
    return 0;
}

void partitionTransfer(graphInfo *info, graphAccelerator * acc)
{
    // Synchronize buffer content with device side
    acc->outDegBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    for (int i = 0; i < info->partitionNum; i++) {
        for (int j = 0; j < SUB_PARTITION_NUM; j++) {
            acc->propBuffer[j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
            acc->edgeBuffer[i][j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
            acc->propApplyBuffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }

    std::cout << "[INFO] Sync data (prop, temp, edge, outDeg) to device " << std::endl;
}

void setAccKernelArgs(graphInfo *info, graphAccelerator * acc)
{
    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
            acc->gsRun[sp] = xrt::run(acc->gsKernel[sp]);
            // acc->gsRun[sp].set_arg(0, acc->edgeBuffer[p][sp]);
            // // acc->gsRun[sp].set_arg(1, acc->propBuffer[sp]);
            // acc->gsRun[sp].set_arg(2, acc->tempBuffer[p][sp]);
            // acc->gsRun[sp].set_arg(3, info->chunkProp[p][sp].edgeNumChunk * 2);
            acc->gsRun[sp].set_arg(4, 0);
            acc->gsRun[sp].set_arg(5, info->alignedCompressedVertexNum);
        }
    }

#if USE_APPLY

    for (int p = 0; p < info->partitionNum; p++) {
        for (int sp = 0; sp < SUB_PARTITION_NUM - 1; sp++) {
            acc->readRun[sp] = xrt::run(acc->readKernel[sp]);
            // acc->readRun[sp].set_arg(0, acc->tempBuffer[p][sp]);
            // acc->readRun[sp].set_arg(2, info->chunkProp[p][sp].destVertexNumChunk);
            acc->mergeRun[sp] = xrt::run(acc->mergeKernel[sp]);
            // acc->mergeRun[sp].set_arg(3, 0); // dest = 0;
            // acc->mergeRun[sp].set_arg(4, info->chunkProp[p][sp].destVertexNumChunk);
            acc->forwardRun[sp] = xrt::run(acc->forwardKernel[sp]);
            // acc->forwardRun[sp].set_arg(3, 1); // dest = 1;
            // acc->forwardRun[sp].set_arg(4, info->chunkProp[p][sp].destVertexNumChunk);
            acc->writeRun[sp] = xrt::run(acc->writeKernel[sp]);
            // acc->writeRun[sp].set_arg(0, acc->newBuffer[p][sp]); // need to add offset
            // acc->writeRun[sp].set_arg(2, info->chunkProp[p][sp].destVertexNumChunk);
        }

        acc->readRun[SUB_PARTITION_NUM - 1] = xrt::run(acc->readKernel[SUB_PARTITION_NUM - 1]);
        // acc->readRun[SUB_PARTITION_NUM - 1].set_arg(0, acc->tempBuffer[p][SUB_PARTITION_NUM - 1]);
        // acc->readRun[SUB_PARTITION_NUM - 1].set_arg(2, info->chunkProp[p][SUB_PARTITION_NUM - 1].destVertexNumChunk);
        acc->applyRun = xrt::run(acc->applyKernel);
        // acc->applyRun.set_arg(0, acc->propBuffer[0]);
        acc->applyRun.set_arg(3, acc->outDegBuffer);
        acc->applyRun.set_arg(4, acc->outRegBuffer);
        // acc->applyRun.set_arg(5, info->chunkProp[p][0].destVertexNumChunk); // depends on which SLR
        acc->applyRun.set_arg(6, 0);
        acc->applyRun.set_arg(7, 0);
        acc->writeRun[SUB_PARTITION_NUM - 1] = xrt::run(acc->writeKernel[SUB_PARTITION_NUM - 1]);
        // acc->writeRun[SUB_PARTITION_NUM - 1].set_arg(0, acc->subNewBuffer[p][SUB_PARTITION_NUM - 1]); // need to add offset
        // acc->writeRun[SUB_PARTITION_NUM - 1].set_arg(2, info->chunkProp[p][SUB_PARTITION_NUM - 1].destVertexNumChunk);
    }

#endif

    std::cout << "[INFO] Set Initial Kernel args done" << std::endl;

}

void resultTransfer(graphInfo *info, graphAccelerator * acc, int run_counter)
{
    // Transfer device buffer content to host side
    acc->outRegBuffer.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {
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

