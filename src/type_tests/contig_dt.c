#include <stdio.h>
#include <mpi.h>

struct {
    short x; 
    short y; 
    short z;
    short t;
} point;

/* Run with four processes */
void main(int argc, char *argv[]) {
    int rank;
    MPI_Status status;
    MPI_Datatype ptype;
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
 
    MPI_Type_contiguous(4, MPI_INT, &ptype);
    MPI_Type_commit(&ptype);

    if(rank == 0) {
        point.x = 15; point.y = 23; point.z = 6; point.t = 9;
        MPI_Send(&point, 1, ptype, 1, 52, MPI_COMM_WORLD);
    } else if (rank == 1){
        MPI_Recv(&point, 1, ptype, 0, 52, MPI_COMM_WORLD, &status);
        printf("P:%d received coords are (%d, %d, %d, %d)\n", 
                rank, point.x, point.y, point.z, point.t);
    }
    MPI_Finalize();
}
