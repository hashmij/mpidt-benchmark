# Paths
P_DIR=$(PWD)
SRC_DIR=$(PWD)/src
OBJ_DIR=$(PWD)/obj
EXE_DIR=$(PWD)/bin

# Dependecy libraries
MPI_INSTALL=/home/hashmij/dt-work/mvapich2x/install
#MPI_INSTALL=/home/hashmij/xpmem-work/mvapich2/install
XPMEM_INSTALL=/opt/xpmem

# Compiler 
CC = $(MPI_INSTALL)/bin/mpicc

# Executables 
SHM                 = $(SRC_DIR)/shmem_dt.c
SHM_PIPELINE        = $(SRC_DIR)/shmem_pipeline_dt.c
XPM                 = $(SRC_DIR)/xpmem_dt.c
XPM_CACHE           = $(SRC_DIR)/xpmem_cache_dt.c
CMA                 = $(SRC_DIR)/cma_dt.c
BENCH               = $(SRC_DIR)/bench.c
OSU_LATENCY_CONTIG  = $(SRC_DIR)/mpi/osu_latency_contig.c

CFLAGS = -g -Wall -I$(P_DIR)/include -I$(XPMEM_INSTALL)/include -I$(MPI_INSTALL)/include
LIBS = $(MPI_INSTALL)/lib/libmpi.so $(XPMEM_INSTALL)/lib/libxpmem.so

all: shmem_dt.x shmem_pipeline_dt.x xpmem_dt.x xpmem_cache_dt.x cma_dt.x bench.x \
	osu_latency_contig.x 
	#osu_latency_vector.x	

# Non-MPI benchmarks
shmem_dt.x:	$(SHM)
	$(CC) $(CFLAGS) -o $@ $(SHM) $(LIBS)
shmem_pipeline_dt.x:	$(SHM_PIPELINE)
	$(CC) $(CFLAGS) -o $@ $(SHM_PIPELINE) $(LIBS)
xpmem_dt.x:	$(XPM)
	$(CC) $(CFLAGS) -o $@ $(XPM) $(LIBS)
xpmem_cache_dt.x:	$(XPM_CACHE)
	$(CC) $(CFLAGS) -o $@ $(XPM_CACHE) $(LIBS)
cma_dt.x:	$(CMA)
	$(CC) $(CFLAGS) -o $@ $(CMA) $(LIBS)
bench.x:	$(BENCH)
	$(CC) $(CFLAGS) -o $@ $(BENCH) $(LIBS)


# compile util files
	$(CC) $(CFLAGS) -c $(SRC_DIR)/mpi/osu_util.c $(LIBS)

# Modified OSU Benchmarks for datatype
osu_latency_contig.x: $(OSU_LATENCY_CONTIG) 
	$(CC) $(CFLAGS) -o $@ osu_util.o $(OSU_LATENCY_CONTIG) $(LIBS)

# Move executables to ./bin and object files to ./obj
	mv *.o $(OBJ_DIR)/
	mv *.x $(EXE_DIR)/

clean:
	rm $(EXE_DIR)/*
	rm $(OBJ_DIR)/*
