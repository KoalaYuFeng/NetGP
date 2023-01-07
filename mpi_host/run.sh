echo "run MG"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d MG

echo "run FU"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d FU

echo "run TC"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d TC

echo "run PK"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d PK

echo "run WP"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d WP

echo "run HW"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d HW

echo "run LJ"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d LJ

echo "run R19"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d R19

echo "run R21"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d R21

echo "run R24"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d R24

echo "run G23"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d G23

echo "run G24"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d G24

echo "run TW"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d TW

echo "run G25"
/opt/openmpi/bin/mpirun -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/opt/xillinx/xrt/lib/ -x XILINX_XRT=/home/opt/xilinx/xrt -n 2 --hostfile host_file ./mpi_host -d G25