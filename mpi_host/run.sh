/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d MG

