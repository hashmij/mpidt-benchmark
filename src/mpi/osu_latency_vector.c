#define BENCHMARK "OSU MPI%s Latency Test"
/*
 * Copyright (C) 2002-2018 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */
#include <osu_util.h>

#define VALIDATE 0
#define DEBUG_PRINT 0

const int ROWS = 512;
const int COLS = 512;

int main (int argc, char *argv[])
{
    int myid, numprocs, i, j, k;
    int cnt;
    MPI_Status reqstat;
    char *s_buf, *r_buf;
    double t_start = 0.0, t_end = 0.0;
    int po_ret = 0;
    options.bench = PT2PT;
    options.subtype = LAT;

    set_header(HEADER);
    set_benchmark_name("osu_latency");

    po_ret = process_options(argc, argv);

    if (PO_OKAY == po_ret && NONE != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &numprocs));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &myid));
    

    if (0 == myid) {
        switch (po_ret) {
            case PO_CUDA_NOT_AVAIL:
                fprintf(stderr, "CUDA support not enabled.  Please recompile "
                        "benchmark with CUDA support.\n");
                break;
            case PO_OPENACC_NOT_AVAIL:
                fprintf(stderr, "OPENACC support not enabled.  Please "
                        "recompile benchmark with OPENACC support.\n");
                break;
            case PO_BAD_USAGE:
                print_bad_usage_message(myid);
                break;
            case PO_HELP_MESSAGE:
                print_help_message(myid);
                break;
            case PO_VERSION_MESSAGE:
                print_version_message(myid);
                MPI_CHECK(MPI_Finalize());
                exit(EXIT_SUCCESS);
            case PO_OKAY:
                break;
        }
    }

    switch (po_ret) {
        case PO_CUDA_NOT_AVAIL:
        case PO_OPENACC_NOT_AVAIL:
        case PO_BAD_USAGE:
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_FAILURE);
        case PO_HELP_MESSAGE:
        case PO_VERSION_MESSAGE:
            MPI_CHECK(MPI_Finalize());
            exit(EXIT_SUCCESS);
        case PO_OKAY:
            break;
    }

    if(numprocs != 2) {
        if(myid == 0) {
            fprintf(stderr, "This test requires exactly two processes\n");
        }

        MPI_CHECK(MPI_Finalize());
        exit(EXIT_FAILURE);
    }

    /* declare and commit point type */
    MPI_Datatype coltype;
    MPI_Type_vector(ROWS, 1, COLS, MPI_INT, &coltype);
    MPI_Type_commit(&coltype);
    MPI_Aint extent;
    MPI_Type_extent (coltype, &extent);
    printf("[%d]: Type extent: %ld, max bytes %ld \n", myid, extent, extent * COLS);
   
    size_t alignment = sysconf(_SC_PAGESIZE);

    if (posix_memalign((void**)&s_buf, alignment, ROWS * COLS * )) {
        fprintf(stderr, "Error allocating host memory\n");
        return 1;
    }

    if (posix_memalign((void**)&r_buf, alignment, COLS * extent)) {
        fprintf(stderr, "Error allocating host memory\n");
        return 1;
    }
 
    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));   
    
    print_header(myid, LAT);
     
    /* Latency test */
    for(cnt = options.min_message_size; cnt <= COLS; cnt = (cnt ? cnt * 2 : 1)) {
        for (j = 0; j < ROWS; j++) {
            for (k = 0; k < COLS; k++) {
                (((int *)s_buf) + (j*cols+k)) = 1;
                (((int *)r_buf) + (j*cols+k)) = 0;
            }
        } 
                        

        if(cnt > LARGE_MESSAGE_SIZE/extent) {
            options.iterations = options.iterations_large;
            options.skip = options.skip_large;
        }

        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

        if(myid == 0) {
            for(i = 0; i < options.iterations + options.skip; i++) {
                if(i == options.skip) {
                    t_start = MPI_Wtime();
                }

                MPI_CHECK(MPI_Send(s_buf, cnt, coltype, 1, 1, MPI_COMM_WORLD));
                MPI_CHECK(MPI_Recv(r_buf, cnt, coltype, 1, 1, MPI_COMM_WORLD, &reqstat));
            }

            t_end = MPI_Wtime();
        }

        else if(myid == 1) {
            for(i = 0; i < options.iterations + options.skip; i++) {
                MPI_CHECK(MPI_Recv(r_buf, cnt, ptype, 0, 1, MPI_COMM_WORLD, &reqstat));
                MPI_CHECK(MPI_Send(s_buf, cnt, ptype, 0, 1, MPI_COMM_WORLD));
      
            }
        }

#if VALIDATE
        if (myid == 1) {
            int err=0;
                for (j = 0; j < cnt; j++) {
                    if ( !( (((struct point *)r_buf) + j)->x == 1 &&
                         (((struct point *)r_buf) + j)->y == 2 && 
                         (((struct point *)r_buf) + j)->z == 3 && 
                         (((struct point *)r_buf) + j)->t == 4 ) ) {

                        err++;
                    }
#if DEBUG_PRINT 
                    printf ("[%d]: rbuf: x %d y %d z %d t %d\n", myid, 
                                (((struct point *)r_buf) + j)->x, 
                                (((struct point *)r_buf) + j)->y, 
                                (((struct point *)r_buf) + j)->z, 
                                (((struct point *)r_buf) + j)->t);
#endif                    
                }   

                if (err > 0) {
                    fprintf (stderr, "[%d]: %d errors found ...\n", myid, err);
                } else {
                    fprintf (stdout, "[%d]: Validation successfull ...\n", myid);
                }
        }
#endif

        if(myid == 0) {
            double latency = (t_end - t_start) * 1e6 / (2.0 * options.iterations);

            fprintf(stdout, "%-*d%*.*f\n", 10, cnt, FIELD_WIDTH,
                    FLOAT_PRECISION, latency);
            fflush(stdout);
        }
    }

    free(s_buf);
    free(r_buf);

    MPI_CHECK(MPI_Finalize());

    if (NONE != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}

