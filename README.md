## Introduction

NetGP: The extended version of ThunderGP, integrate application-specific network module in ThunderGP for multi-FPGA execution. 

## Prerequisites
* The gcc-9.4
* Tools:
    * Xilinx Vitis 2021.2 Design Suit
* Evaluated platforms from Xilinx:
    * Xilinx Alveo U250 Data Center Accelerator Card

## Run the code
NetGP currently has seven build-in graph algorithms: PageRank (PR), Sparse Matrix-Vector Multiplication (SpMV), Breadth-First Search (BFS), Single Source Shortest Path (SSSP), Closeness Centrality (CC), ArticleRank (AR), and Weakly Connected Component (WCC). 
The desired application can be implemented by passing argument ```app=[the algorithm]``` to ``` make ``` command. The below table is for quick reference.

| Argument    | Accelerated algorithm  |
|--------------|--------------|
| ```app=pr``` | PageRank (PR)|
| ```app=spmv``` | Sparse Matrix-vector Multiplication (SpMV) |
| ```app=bfs``` | Breadth First Search (BFS)|
| ```app=sssp``` | Single Source Shortest Path (SSSP)|
| ```app=cc``` | Closeness Centrality (CC)|
| ```app=ar``` | ArticleRank  (AR)|
| ```app=wcc``` | Weakly Connected Component  (WCC)|

Besides, NetGP has two types of nodes: GAS node and GS node for multi-node execution. Pass argument ```node_type=[the node type]``` to ``` make ``` command.
| Argument    | Accelerated algorithm  |
|--------------|--------------|
| ```node_type=gas``` | node with Apply, Sync, Merge, Forward, GatherScatter, Write kernels |
| ```node_type=gs``` | node with Merge, Forward, GatherScatter, Write kernels |

#### Here is the example of implementing the accelerator for PageRank on Alveo U250 platform with Vitis 2021.2. 
```sh
$ vim ThunderGP.mk 
$ # configure the DEVICE as DEVICES := xilinx_u250_gen3x16_xdma_3_1_202020_1; configure TARGETS := hw
$ make app=pr node_type=gas all # make the FPGA bitstream. It takes time :) around 12 hours.
# For execution on real hardware. The path of graph dataset needs to be provided by the user. 
```