#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

const int rows = 4096;
const int cols = 4096;

void validate (int rank, int buf[rows][cols], const int refcol, int val) {
    int err = 0;
    int i, j;
   
    for (i = 0; i < rows; i++) {
        if (buf[i][refcol] != val) {
            err++;
        }
    }

    if(err > 0) {
        printf ("[%d]: %d errors found ... \n", rank, err);
    } else {
        printf ("[%d]: validation successful !!!\n", rank);
    }
}

void main(int argc, char *argv[]) {
    int rank,i,j;
    MPI_Status status;
    int x[rows][cols];
    MPI_Datatype coltype;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
   
    /* int MPI_Type_vector(int count (or numblocks), int blocklength, int stride, 
                MPI_Datatype oldtype, MPI_Datatype *newtype)
    */
    MPI_Type_vector(rows, 1, cols, MPI_INT, &coltype);
    MPI_Type_commit(&coltype);

    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            x[i][j] = 0;
        }
    }
    
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        for (i = 0; i < rows; i++) {
            x[i][2] = 5;
        }


        //print
#if 0
        for (i = 0; i < rows; i++) {
            for (j = 0; j < cols; j++) {
                printf("%d \t", x[i][j]);
            }
            printf ("\n");
        }
#endif

        MPI_Send(&x[0][2],1,coltype,1,52,MPI_COMM_WORLD);
    } else if(rank==1) {
        MPI_Recv(&x[0][3],1,coltype,0,52,MPI_COMM_WORLD,&status);
   
        validate(rank, x, 3 /*reference column */, 5 /*reference value */);
#if 0
        for (i = 0; i < rows; i++) {
            for (j = 0; j < cols; j++) {
                printf("%d \t", x[i][j]);
            }
            printf ("\n");
        }
#endif
    }
    
    MPI_Finalize();
}
