#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>


void GS_run (int id, int partition) {
  usleep(50000); // replace it with GS execution code.
  std::cout << "Gs run at processor " << id << " partition " << partition << std::endl;
}

int main(int argc, char** argv) {

  int num_elements_per_proc = 2; // for two processor;

  MPI_Init(NULL, NULL);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  std::cout << "function start at " << world_rank << " in " << world_size << std::endl;

  int p_iteration = 10; // overall partition number;
  int p_recv[1] = {-1}; // received partition id;
  int p_id_tx[2] = {0, 0}; // partition id for sending in root node, start at partition 0;

  for (int p = 0; p < p_iteration;) {

    MPI_Scatter(p_id_tx, 1, MPI_INT, p_recv, 1, MPI_INT, 0, MPI_COMM_WORLD);

    GS_run(world_rank, p_recv[0]);

    int p_temp_rx[2] = {-1};
    MPI_Gather(&(p_recv[0]), 1, MPI_INT, p_temp_rx, 1, MPI_INT, 0, MPI_COMM_WORLD); // * sync operation *

    if (world_rank == 0) {
      if (p_temp_rx[0] == p_temp_rx[1]) { // check whether receive the right partition id;
        p_id_tx[0]++;
        p_id_tx[1]++;
        p++;
      }
    } else {
      p++;
    }

  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
}
