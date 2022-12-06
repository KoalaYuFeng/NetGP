
#include "host_graph_sw.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"


#define HW_EMU_DEBUG        (0)
#define HW_EMU_DEBUG_SIZE   (16384 * 4)

void kernelInit(graphAccelerator * acc)
{

//     std::string id, krnl_name;
//     xrt::kernel krnl_gs[SUB_PARTITION_NUM];
//     for (int i = 0; i < SUB_PARTITION_NUM; i++)
//     {
//         id = std::to_string(i + 1);
//         krnl_name = "readEdgesCU1:{readEdgesCU1_" + id + "}";
//         krnl_gs[i] = xrt::kernel(acc->graphDevice, acc->graphUuid, krnl_name.c_str());
//         acc->gsKernel[i] =  krnl_gs[i];

//     }

// #if HAVE_APPLY

//     std::string krnl_name_full = "vertexApply:{vertexApply_1}";
//     xrt::kernel krnl_apply = xrt::kernel(acc->graph_device, acc->graph_uuid, krnl_name_full.c_str());
//     acc->applyKernel = krnl_apply;

// #endif

//     std::cout << " accelerator init done ! " << std::endl;
}

void setGsKernel(int partId, int superStep, graphInfo *info)
{
//     int currentPropId = superStep % 2;

//     for (int i = 0; i < SUB_PARTITION_NUM; i++)
//     {
//         gatherScatterDescriptor * gsHandler = getGatherScatter(i);
//         subPartitionDescriptor * partition = getSubPartition(partId * SUB_PARTITION_NUM + i);
//         int argvi = 0;
//         int edgeEnd     = (partition->listEnd) * 2;
//         int sinkStart   = 0;
//         int sinkEnd     = MAX_VERTICES_IN_ONE_PARTITION;

// #if HW_EMU_DEBUG
//         edgeEnd         = HW_EMU_DEBUG_SIZE;
// #endif

//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(partition->edgeHead.id));
//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(gsHandler->prop[currentPropId].id));
//         //clSetKernelArg(gsHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(partition->edgeTail.id));

//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(partition->tmpProp.id));
//         he_set_dirty(partition->tmpProp.id);

// #if HAVE_EDGE_PROP
//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(partition->edgeProp.id));
// #endif
//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(int),    &edgeEnd);
//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(int),    &sinkStart);
//         clSetKernelArg(gsHandler->kernel, argvi++, sizeof(int),    &sinkEnd);
//     }
}

#if  CUSTOMIZE_APPLY == 0


void setApplyKernel(int partId, int superStep, graphInfo *info)
{
// #if HAVE_APPLY
//     int currentPropId = superStep % 2;
//     int updatePropId  = (superStep + 1) % 2;

//     applyDescriptor * applyHandler = getApply();
//     int argvi = 0;
//     unsigned int argReg = dataPrepareGetArg(info);
//     subPartitionDescriptor *p_partition = getSubPartition(partId * SUB_PARTITION_NUM);

//     volatile unsigned int partitionVertexNum = ((p_partition->dstVertexEnd - p_partition->dstVertexStart)
//             / (ALIGN_SIZE ) + 1) * (ALIGN_SIZE );
//     int sink_end = partitionVertexNum;
//     int offset = p_partition->dstVertexStart;


//     clSetKernelArg(applyHandler->kernel, argvi++, sizeof(cl_mem),
//                    get_cl_mem_pointer(getGatherScatter(0)->prop[currentPropId].id));

//     for (int i = 0; i < SUB_PARTITION_NUM; i++)
//     {
//         clSetKernelArg(applyHandler->kernel, argvi++, sizeof(cl_mem),
//                        get_cl_mem_pointer(getSubPartition(partId * SUB_PARTITION_NUM + i)->tmpProp.id)
//                       );
//     }
//     for (int i = 0; i < SUB_PARTITION_NUM; i++)
//     {
//         clSetKernelArg(applyHandler->kernel, argvi++, sizeof(cl_mem),
//                        get_cl_mem_pointer(getGatherScatter(i)->prop[updatePropId].id)
//                       );
//         he_set_dirty(getGatherScatter(i)->prop[updatePropId].id);
//     }

// #if HAVE_APPLY_OUTDEG
//     clSetKernelArg(applyHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(MEM_ID_OUT_DEG));
// #endif
//     clSetKernelArg(applyHandler->kernel, argvi++, sizeof(cl_mem), get_cl_mem_pointer(MEM_ID_RESULT_REG));
//     he_set_dirty(MEM_ID_RESULT_REG);

//     clSetKernelArg(applyHandler->kernel, argvi++, sizeof(int),    &sink_end);
//     clSetKernelArg(applyHandler->kernel, argvi++, sizeof(int),    &offset);
//     clSetKernelArg(applyHandler->kernel, argvi++, sizeof(int),    &argReg);
// #endif
}

#endif
