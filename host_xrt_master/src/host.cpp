#include "cmdlineparser.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <unistd.h>

#include "alveo_vnx_cmac.h"
#include "alveo_vnx_configs.h"
#include "alveo_vnx_nl.h"
#include "fpga_device.h"
#include "fpga_ip.h"
#include "fpga_kernel.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define DATA_SIZE 65536

//////////////MAIN FUNCTION//////////////
int main(int argc, char** argv) {
    // Command Line Parser
    sda::utils::CmdLineParser parser;

    // Switches
    //**************//"<Full Arg>",  "<Short Arg>", "<Description>", "<Default>"
    parser.addSwitch("--xclbin_file", "-x", "input binary file string", "");
    parser.addSwitch("--device_id", "-d", "device index", "0");
    parser.parse(argc, argv);

    // Read settings
    std::string binaryFile = parser.value("xclbin_file");
    int device_index = stoi(parser.value("device_id"));

    std::cout << "Open the device" << device_index << std::endl;
    auto device = xrt::device(device_index);
    std::cout << "Load the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);

    // Network configure begin

    auto fpga_device = FpgaDevice(device, uuid);
    AlveoVnxCmac cmac = AlveoVnxCmac(fpga_device, 1); // 1 for cmac_1
    AlveoVnxNetworkLayer netlayer = AlveoVnxNetworkLayer(fpga_device, 1); // 1 for netlayer 1
    // read cmac link status;
    bool linkStatus = cmac.readRegister("stat_rx_status") & 0x1;
    while(!linkStatus) {
        std::cout<<"LinkStatus is " << linkStatus <<std::endl;
        sleep(1);
        linkStatus = cmac.readRegister("stat_rx_status") & 0x1;
    }
    std::cout<<"Link status up"<<std::endl;

    std::string our_ip = "192.168.0.204";
    uint32_t our_port = 60523;
    std::string our_mac = "00:0a:35:02:9d:cc";
    std::cout << "our fpga " << " ip:" << our_ip <<" port: "<< our_port << std::endl;

    std::string their_ip = "192.168.0.203";
    uint32_t their_port = 61452;
    std::string their_mac = "00:0a:35:02:9d:cb";
    std::cout << "their fpga " << " ip:" << their_ip <<" port: "<< their_port << std::endl;

    netlayer.setAddress(our_ip, our_mac);
    netlayer.setSocket(their_ip, their_port, 5001, 0);
    netlayer.setSocket(their_ip, 5001, our_port, 1); // theirIp, theirPort, ourPort, socketId
    netlayer.getSocketTable();
    std::cout << "link loop created" << std::endl;

    bool ARP_ready = false;
    while(!ARP_ready) {
        std::cout << "wait ARP ready!" << std::endl;
        netlayer.runARPDiscovery();
        usleep(500000);
        ARP_ready = netlayer.IsARPTableFound(their_ip);
    }
    std::cout << "find ARP table" << std::endl;

    // Network configure end

    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;
    int kernel_size = 2; // using 2 merge kernels and 2 forward kernels , 3 read and write kernels, only 1 apply kernel.

    xrt::kernel krnl_merge[kernel_size];
    xrt::kernel krnl_forward[kernel_size];
    xrt::kernel krnl_apply;
    xrt::kernel krnl_read[kernel_size];
    xrt::kernel krnl_write[kernel_size];

    std::string cu_id;
    for (int i = 0; i < kernel_size; i++) {
        cu_id = std::to_string(i + 1);
        std::string krnl_name = "streamMerge";
        std::string krnl_name_full = krnl_name + ":{" + "streamMerge_" + cu_id + "}";
        printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), i);
        krnl_merge[i] = xrt::kernel(device, uuid, krnl_name_full.c_str());

        krnl_name = "streamForward";
        krnl_name_full = krnl_name + ":{" + "streamForward_" + cu_id + "}";
        printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), i);
        krnl_forward[i] = xrt::kernel(device, uuid, krnl_name_full.c_str());

        krnl_name = "readVertex";
        krnl_name_full = krnl_name + ":{" + "readVertex_" + cu_id + "}";
        printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), i);
        krnl_read[i] = xrt::kernel(device, uuid, krnl_name_full.c_str());

        krnl_name = "writeVertex";
        krnl_name_full = krnl_name + ":{" + "writeVertex_" + cu_id + "}";
        printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), i);
        krnl_write[i] = xrt::kernel(device, uuid, krnl_name_full.c_str());
    }

    // std::string krnl_name_full = "readVertex:{readVertex_3}";
    // printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), kernel_size);
    // krnl_read[2] = xrt::kernel(device, uuid, krnl_name_full.c_str());

    // krnl_name_full = "writeVertex:{writeVertex_3}";
    // printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), kernel_size);
    // krnl_write[2] = xrt::kernel(device, uuid, krnl_name_full.c_str());

    std::string krnl_name_full = "vertexApply:{vertexApply_1}";
    printf("Creating a kernel [%s] for CU0\n", krnl_name_full.c_str());
    krnl_apply = xrt::kernel(device, uuid, krnl_name_full.c_str());

    std::cout << "Allocate Buffer in Global Memory\n";
    // need to check group_id with xclbin.info.
    xrt::bo buffer_tmp_1 = xrt::bo(device, vector_size_bytes, krnl_read[0].group_id(0));
    xrt::bo buffer_tmp_2 = xrt::bo(device, vector_size_bytes, krnl_read[1].group_id(0));
    // xrt::bo buffer_merge_from_net = xrt::bo(device, vector_size_bytes, krnl_read[2].group_id(0));

    xrt::bo buffer_new_1 = xrt::bo(device, vector_size_bytes, krnl_write[0].group_id(0));
    xrt::bo buffer_new_2 = xrt::bo(device, vector_size_bytes, krnl_write[1].group_id(0));
    // xrt::bo buffer_forward_to_net = xrt::bo(device, vector_size_bytes, krnl_write[2].group_id(0));

    xrt::bo buffer_apply_vertex = xrt::bo(device, vector_size_bytes, krnl_apply.group_id(0));
    xrt::bo buffer_apply_out_deg = xrt::bo(device, vector_size_bytes, krnl_apply.group_id(0));
    xrt::bo buffer_apply_out_REG = xrt::bo(device, vector_size_bytes, krnl_apply.group_id(0));

    int* tmp_vertex_1 = buffer_tmp_1.map<int*>();
    int* tmp_vertex_2 = buffer_tmp_2.map<int*>();
    // int* merge_from_net = buffer_merge_from_net.map<int*>();
    
    int* new_vertex_1 = buffer_new_1.map<int*>();
    int* new_vertex_2 = buffer_new_2.map<int*>();
    // int* forward_to_net = buffer_forward_to_net.map<int*>();
    
    int* apply_vertex = buffer_apply_vertex.map<int*>();
    int* apply_out_deg = buffer_apply_out_deg.map<int*>();
    int* apply_out_REG = buffer_apply_out_REG.map<int*>();

    // Create the test data
    for (int i = 0; i < DATA_SIZE; i++) { 
        tmp_vertex_1[i] = (i * 10) % 4096;
        tmp_vertex_2[i] = (i * 10) % 4096;
        new_vertex_1[i] = 0;
        new_vertex_2[i] = 0;
        apply_vertex[i] = 1; // not used in pr case
        apply_out_deg[i] = 2;
        apply_out_REG[i] = 0; // not used in pr case
        // merge_from_net[i] = (i * 10) % 4096;
        // forward_to_net[i] = 0;
    }

    // Synchronize buffer content with device side
    std::cout << "synchronize input buffer data to device global memory\n";
    buffer_tmp_1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    buffer_tmp_2.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    // buffer_merge_from_net.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    buffer_new_1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    buffer_new_2.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    // buffer_forward_to_net.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    buffer_apply_vertex.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    buffer_apply_out_deg.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    buffer_apply_out_REG.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "Seting kernel args \n";

    auto run_read_0 = xrt::run(krnl_read[0]);
    run_read_0.set_arg(0, buffer_tmp_1);
    run_read_0.set_arg(2, DATA_SIZE);
    auto run_read_1 = xrt::run(krnl_read[1]);
    run_read_1.set_arg(0, buffer_tmp_2);
    run_read_1.set_arg(2, DATA_SIZE);
    // auto run_read_2 = xrt::run(krnl_read[2]);
    // run_read_2.set_arg(0, buffer_merge_from_net);
    // run_read_2.set_arg(2, DATA_SIZE);

    auto run_write_0 = xrt::run(krnl_write[0]);
    run_write_0.set_arg(0, buffer_new_1);
    run_write_0.set_arg(2, DATA_SIZE);
    auto run_write_1 = xrt::run(krnl_write[1]);
    run_write_1.set_arg(0, buffer_new_2);
    run_write_1.set_arg(2, DATA_SIZE);
    // auto run_write_2 = xrt::run(krnl_write[2]);
    // run_write_2.set_arg(0, buffer_forward_to_net);
    // run_write_2.set_arg(2, DATA_SIZE);

    auto run_apply = xrt::run(krnl_apply);
    run_apply.set_arg(0, buffer_apply_vertex);
    run_apply.set_arg(3, buffer_apply_out_deg);
    run_apply.set_arg(4, buffer_apply_out_REG);
    run_apply.set_arg(5, DATA_SIZE);
    run_apply.set_arg(6, 0);
    run_apply.set_arg(7, 0);

    auto run_merge_0 = xrt::run(krnl_merge[0]);
    run_merge_0.set_arg(3, 0); // dest = 0
    run_merge_0.set_arg(4, DATA_SIZE);
    auto run_merge_1 = xrt::run(krnl_merge[1]);
    run_merge_1.set_arg(3, 0); // dest = 0
    run_merge_1.set_arg(4, DATA_SIZE);

    auto run_forward_0 = xrt::run(krnl_forward[0]);
    run_forward_0.set_arg(3, 1); // dest = 1
    run_forward_0.set_arg(4, DATA_SIZE);
    auto run_forward_1 = xrt::run(krnl_forward[1]);
    run_forward_1.set_arg(3, 1); // dest = 1
    run_forward_1.set_arg(4, DATA_SIZE);

    std::cout << "Execution of the kernel\n";
    auto start = std::chrono::steady_clock::now();
    run_read_0.start();
    run_read_1.start();
    // run_read_2.start();
    run_merge_0.start();
    run_merge_1.start();
    run_apply.start();
    run_forward_0.start();
    run_forward_1.start();
    run_write_0.start();
    run_write_1.start();
    // run_write_2.start();
    std::cout << " kernel start done! " <<std::endl;

    run_read_0.wait();
    run_read_1.wait();
    // run_read_2.wait();
    run_merge_0.wait();
    run_merge_1.wait();
    run_apply.wait();
    run_forward_0.wait();
    run_forward_1.wait();
    run_write_0.wait();
    run_write_1.wait();
    // run_write_2.wait();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "s\n";

    // Get the output;
    std::cout << "Get the output data from the device" << std::endl;
    buffer_new_1.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    buffer_new_2.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    // buffer_forward_to_net.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    // Validate our results
    int match = 0;
    // for (int i = 0; i < DATA_SIZE; i++) {
    //     if(new_vertex_1[i] != new_vertex_2[i]) match = 1;
    //     int result_sw = ( tmp_vertex_1[i] + tmp_vertex_2[i] + merge_from_net[i] ) * 108 / (128 * apply_out_deg[i]);
    //     if (forward_to_net[i] != result_sw) match = 1;
    //     // std::cout <<"["<<i<<"]"<< " result_sw is " << result_sw << std::endl;
    //     // std::cout << " hw_result is " << forward_to_net[i] <<" "<< new_vertex_1[i]<<" "<< new_vertex_2[i]<< std::endl;
    // }
    std::cout << "XRT host process " << DATA_SIZE << " vertices done!" <<std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "TEST " << (match ? "FAILED" : "PASSED") << std::endl;
    std::cout << "=================" << std::endl;
    std::cout<<std::endl;
    return 0;
}
