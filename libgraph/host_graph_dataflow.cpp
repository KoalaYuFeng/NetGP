
#include "host_graph_sw.h"
#include "host_graph_scheduler.h"
#include "host_graph_api.h"
#include <cstring>

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

// graphAccelerator thunderGraph;

// graphAccelerator * getAccelerator(void)
// {
//     return &thunderGraph;
// }

int acceleratorInit(std::string& file_name,  graphInfo *info, graphAccelerator* acc)
{

    // load xclbin file
    acc->graphDevice = xrt::device(0); // every VM has only one FPGA, id = 0;
    acc->graphUuid = acc->graphDevice.load_xclbin(file_name);

    std::cout << "xclbin load done" <<std::endl;

    // init FPGA kernels
    std::string id, krnl_name;
    for (int i = 0; i < SUB_PARTITION_NUM; i++)
    {
        id = std::to_string(i + 1);
        krnl_name = "readEdgesCU1:{readEdgesCU1_" + id + "}";
        acc->gsKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        // krnl_name = "streamMerge:{streamMerge_" + id + "}";
        // acc->mergeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        // krnl_name = "streamForward:{streamForward_" + id + "}";
        // acc->forwardKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        // krnl_name = "readVertex:{readVertex_" + id + "}";
        // acc->readKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
        // krnl_name = "writeVertex:{writeVertex_" + id + "}";
        // acc->writeKernel[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
    }
    // std::string krnl_name_full = "vertexApply:{vertexApply_1}";
    // acc->applyKernel = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name_full.c_str());

    // init FPGA buffers
    long unsigned int prop_size_bytes = 0;
    long unsigned int edge_size_bytes = 0;
    long unsigned int outDeg_size_bytes = 0;

    // // acc->outDegBuffer.resize(info->partitionNum);
    // // acc->outRegBuffer.resize(info->partitionNum);

    // acc->propBuffer.resize(SUB_PARTITION_NUM);
    // acc->tempBuffer.resize(SUB_PARTITION_NUM);
    acc->edgeBuffer.resize(info->partitionNum);
    std::cout << "acc edge buffer number = "<< acc->edgeBuffer.size() << std::endl;

    for (int i = 0; i < info->partitionNum; i++) {
        acc->edgeBuffer[i].resize(SUB_PARTITION_NUM);

        for (int j = 0; j < SUB_PARTITION_NUM; j++) {
            edge_size_bytes = info->chunkProp[i][j].edgeNumChunk * 2 * sizeof(int); // each edge has two vertex, vertex index uses int type;
            acc->edgeBuffer[i][j] = xrt::bo(acc->graphDevice, edge_size_bytes, acc->gsKernel[j].group_id(0));
            info->chunkEdgeData[i][j] = acc->edgeBuffer[i][j].map<int*>();

            prop_size_bytes = info->alignedCompressedVertexNum * sizeof(prop_t); // vertex prop;
            acc->propBuffer[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->gsKernel[j].group_id(1));
            acc->tempBuffer[j] = xrt::bo(acc->graphDevice, prop_size_bytes, acc->gsKernel[j].group_id(1));
            info->chunkTempData[j] = acc->tempBuffer[j].map<int*>();
            info->chunkPropData[j] = acc->propBuffer[j].map<int*>();
        }
        // outDeg_size_bytes = info->chunkProp[i][0].destVertexNumChunk * sizeof(int); // vertex prop;
        // acc->outDegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0)); // vertex number * sizeof(int)
        // acc->outRegBuffer = xrt::bo(acc->graphDevice, outDeg_size_bytes, acc->applyKernel.group_id(0));

        // info->chunkOutDegData = acc->outDegBuffer[i].map<int*>();
        // info->chunkOutRegData = acc->outRegBuffer[i].map<int*>();
    }

    std::cout << " accelerator init done ! " << std::endl;
    return 0;
}

int acceleratorSuperStep(int superStep, graphInfo *info, graphAccelerator * acc)
{
    for (int i = 0; i < info->partitionNum; i++) {
    // for (int i = 0; i < 1; i++) {
        std::cout << "gs kernel start, partition [" << i << "]" <<std::endl;
        for (int j = 0; j < SUB_PARTITION_NUM; j++) {
            int narg = 0;
            acc->gsRun[j] = xrt::run(acc->gsKernel[j]);
            acc->gsRun[j].set_arg(narg++, acc->edgeBuffer[i][j]);
            if (superStep % 2 == 0) {
                acc->gsRun[j].set_arg(narg++, acc->propBuffer[j]);
                acc->gsRun[j].set_arg(narg++, acc->tempBuffer[j]);
            } else {
                acc->gsRun[j].set_arg(narg++, acc->tempBuffer[j]);
                acc->gsRun[j].set_arg(narg++, acc->propBuffer[j]);
            }
            // acc->gsRun[j].set_arg(narg++, DATA_SIZE * 4);
            acc->gsRun[j].set_arg(narg++, info->chunkProp[i][j].edgeNumChunk * 2);
            acc->gsRun[j].set_arg(narg++, 0);
            // acc->gsRun[j].set_arg(narg++, DATA_SIZE);
            acc->gsRun[j].set_arg(narg++, info->alignedCompressedVertexNum);
            // acc->edgeBuffer[i][j].sync(XCL_BO_SYNC_BO_TO_DEVICE); // try to copy data into device memory. map function seems a problem
            acc->gsRun[j].start();
        }

        for (int k = 0; k < SUB_PARTITION_NUM; k++) {
            acc->gsRun[k].wait();

            // std::cout << "gs kernel run end, partition [" << k << "]" <<std::endl;

            // acc->tempBuffer[k].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            // acc->propBuffer[k].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            // for (int size = 0; size < info->alignedCompressedVertexNum; size++) {
            //     std::cout <<" ["<< k << "]["<< size << "]:" << info->chunkTempData[k][size];
            //     if ((size + 1) % 5 == 0) std::cout << std::endl;
            // }

        }

        std::cout << "gs kernel end, partition [" << i << "]" <<std::endl;
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
    for (int i = 0; i < info->partitionNum; i++) {
        for (int j = 0; j < SUB_PARTITION_NUM; j++) {
            // acc->propBuffer[j].write(info->chunkPropData[j]);
            acc->propBuffer[j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
            acc->tempBuffer[j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
            acc->edgeBuffer[i][j].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
    }

    std::cout << "sync data to device " << std::endl;
}

void resultTransfer(graphInfo *info, graphAccelerator * acc, int run_counter)
{
    // Transfer device buffer content to host side
    for (int sp = 0; sp < SUB_PARTITION_NUM; sp++) {

        if (run_counter % 2 == 1) { // ping-pong for temp data and prop data;
            std::fill_n(info->chunkTempData[sp], info->alignedCompressedVertexNum, 0);
            // acc->tempBuffer[sp].write(info->chunkTempData[sp]);
            acc->tempBuffer[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            for (int size = 0; size < info->compressedVertexNum; size++) {
                std::cout <<" ["<< sp << "]["<< size << "]:" << info->chunkTempData[sp][size];
                if ((size + 1) % 5 == 0) std::cout << std::endl;
            }
        } else {
            // acc->propBuffer[sp].write(info->chunkPropData[sp]);
            acc->propBuffer[sp].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
            for (int size = 0; size < info->compressedVertexNum; size++) {
                std::cout <<" ["<< sp << "]["<< size << "]:" << info->chunkPropData[sp][size];
                if ((size + 1) % 5 == 0) std::cout << std::endl;
            }
        }

        std::cout << std::endl;
    }

}

