EXECS=mpi_exe
MPICC?=/opt/openmpi/bin/mpicxx

all: ${EXECS}

mpi_exe: mpi_exe.c
	${MPICC} -o mpi_exe mpi_exe.c

clean:
	rm -f ${EXECS}
