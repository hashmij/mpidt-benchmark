#include <mpi.h>
#include <util.h>

#define CMA_IOV_LIMIT sysconf(_SC_IOV_MAX)

int main(int argc, char **argv)
{
    char *benchmark = "CMA";
    int i, j;
    int iters = NUM_ITER;
    int skip = SKIP_ITER;
    int rank, size;
    pid_t rpid = -1;
    void *lbuf = NULL;
    struct iovec *liov = NULL, *riov = NULL;
    size_t blocksize, bufsize, nblocks;
    size_t page_size;
    double start, end, lat;
    int sender = 0;
    size_t bytes_copied = 0, remaining = 0, total_copied = 0;
    char dummy;
    char *dummybuf = NULL;
    int iov_off = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    assert(argc == 3);

    blocksize = atoi(argv[1]);
    bufsize   = atoi(argv[2]);
    
    nblocks = bufsize / blocksize;

    assert(blocksize > 0);
    assert(bufsize   > 0);
    
    if (rank == sender) {
        rpid = getpid();
    }
    
    lbuf = malloc(bufsize);
    page_size = sysconf(_SC_PAGESIZE);
    posix_memalign(&lbuf, page_size, bufsize);
    memset(lbuf, 'x', bufsize);

    /* create and init iovec list */
    liov = malloc(sizeof(struct iovec) * nblocks);
    riov = malloc(sizeof(struct iovec) * nblocks);
     
    /* initialize iovec list*/
    for (i = 0; i < nblocks; i++) {
        liov[i].iov_base = lbuf + i * blocksize;
        liov[i].iov_len = blocksize;
    }


    /* allocate dummybuf memory and set initial values */
    if (rank == sender) {
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
    }
    
    /* Broadcast the pid of the sender to peer processes */
    MPI_Bcast(&rpid, sizeof(pid_t), MPI_BYTE, sender, MPI_COMM_WORLD);
    

    MPI_Barrier(MPI_COMM_WORLD);

    /* == begin benchmark ==
     * 1. Sender creates the iov list and broadcasts to peer processes.
     * 2. Receiver(s) receive the remote pid and buffer address of the remote buffers.
     * 3. Receiver(s) copy the data directly from the remote iovec.
     * 
     */
    
    for (j = 0; j < iters+skip; j++) {
        
        if (j == skip) start = MPI_Wtime();

        if (rank == sender) {
            /* 1. Send RTS: Broadcast the sender's liov into receivers riov */
            MPI_Bcast(liov, nblocks*sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD);

            /* 2. Wait-for FIN: Sender ensures that everyone has copied the data */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        } else {

            /* 1. Get RTS: Broadcast the sender's liov into receivers riov */
            MPI_Bcast(riov, nblocks*sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD);

            /* 2. Copy remote data */

            iov_off = 0;
            remaining = bufsize;
            total_copied = 0; 
            while (remaining > 0 && iov_off < nblocks) { 
                int curr_blks = (nblocks < CMA_IOV_LIMIT) ? nblocks : CMA_IOV_LIMIT;
                bytes_copied = process_vm_readv(rpid, &liov[iov_off], curr_blks, 
                                    &riov[iov_off], curr_blks, 0);

                total_copied += bytes_copied;
                remaining -= bytes_copied;
                iov_off = (iov_off + curr_blks);
            }
            
            assert (total_copied == bufsize);

            /* 3. Send FIN pkt */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        }    
    } 
    
    end = MPI_Wtime();
    lat = (end - start) * 1e6 / iters;
 
    if(rank == sender) {
        PRINT (benchmark, blocksize, bufsize, lat);
    }
    
    /* free buffer */
    if (lbuf != NULL) { 
        free(lbuf);
    }
    if (dummybuf) {
        free (dummybuf);
    }

    /* destroy iovec list */
    if (liov) {
        free(liov);
    }
    if (riov) {
        free(riov);
    }
       
 
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    
    return 0;
}
