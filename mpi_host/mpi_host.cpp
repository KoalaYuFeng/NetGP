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

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

#define NUM_PROC 2 // for 2 processors;

void GS_Execute (int id, int partition) {
    std::cout << "GS Execution at processor " << id << " partition " << partition << " begin ...";
    // add GS execution code
    std::cout << " ... end" << std::endl;
}

int main(int argc, char** argv) {

    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::cout << "function start at " << world_rank << " in " << world_size << std::endl;

    int p_iteration = 10; // overall partition number;
    int p_recv[1] = {-1}; // received partition id;
    int p_id_tx[NUM_PROC] = {0}; // partition id for sending in root node, start at partition 0;

    for (int p_idx = 0; p_idx < p_iteration;) {

        MPI_Scatter(p_id_tx, 1, MPI_INT, p_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);

        GS_Execute(world_rank, p_recv[0]); // doing GS operation in each partition;

        // std::cout << "Sync partition [" << p_idx << "] ...";
        int p_temp_rx[NUM_PROC];
        for (int i = 0; i < NUM_PROC; i++) {
            p_temp_rx[i] = -1;
        }
        MPI_Gather(&(p_recv[0]), 1, MPI_INT, p_temp_rx, 1, MPI_INT, 0, MPI_COMM_WORLD); // * sync operation *
        // std::cout << " Done." << std::endl;

        if (world_rank == 0) { // for root node
            bool align = true;
            for (int i = 0; i < NUM_PROC - 1; i++) {
                if (p_temp_rx[i] != p_temp_rx[i+1]) align = false;
            }
            if (align) { // check whether receive the right partition id;
                for (int i = 0; i < NUM_PROC; i++) {
                    p_id_tx[i]++;
                }
                p_idx++;
            } else {
                std::cout << "[INFO] ASYNC occur in nodes" << std::endl;
                for (int i = 0; i < NUM_PROC; i++) {
                    std::cout << "Node[" << i << "] 's p_temp_rx is " << p_temp_rx[i] << std::endl;
                }
            }
        } else { // for other nodes
            p_idx++;
        }

    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
