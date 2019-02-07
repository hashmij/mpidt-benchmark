#include <mpi.h>
#include <util.h>


int copy_to_shmem(void *shmem_ptr, 
        struct iovec *liov, 
        size_t blocksize,
        size_t bufsize)
{
    int i, nblocks, offset;
    size_t sum = 0;

    nblocks = bufsize / blocksize;

    offset = 0;
    for (i = 0; i < nblocks; i++) {
        memcpy(shmem_ptr+offset, liov[i].iov_base, liov[i].iov_len);
        offset += liov[i].iov_len;
        sum += liov[i].iov_len; 
    }

    return sum;
}

int copy_from_shmem(void *shmem_ptr,
        struct iovec *liov,
        size_t blocksize,
        size_t bufsize)
{
    int i, nblocks, offset;
    size_t sum = 0;

    nblocks = bufsize / blocksize;

    offset = 0;
    for (i = 0; i < nblocks; i++) {
        memcpy(liov[i].iov_base, shmem_ptr+offset, liov[i].iov_len);
        offset += liov[i].iov_len;
        sum += liov[i].iov_len;
    }

    return sum;
}


int main(int argc, char **argv)
{
    char *benchmark = "SHMEM";
    int i;
    int iters = NUM_ITER;
    int skip = SKIP_ITER;
    int rank, size;
    void *lbuf = NULL;
    void *shmem_ptr;
    struct iovec *liov = NULL;
    size_t blocksize, bufsize, nblocks;
    size_t nread, page_size;
    double start, end, lat;
    int sender = 0;
    int fd = -1;
    char dummy;
    char *dummybuf = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    assert(argc == 3);

    blocksize = atoi(argv[1]);
    bufsize   = atoi(argv[2]);
    
    nblocks = bufsize / blocksize;

    assert(blocksize > 0);
    assert(bufsize   > 0);

    lbuf = malloc(bufsize);
    page_size = sysconf(_SC_PAGESIZE);
    posix_memalign(&lbuf, page_size, bufsize);
    
    if (rank == sender) {
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }

    /* create and init iovec list */
    liov = malloc(sizeof(struct iovec) * nblocks);
    
    /* initialize iovec list */
    for (i = 0; i < nblocks; i++) {
        liov[i].iov_base = lbuf + i * blocksize;
        liov[i].iov_len = blocksize;
    }

    /* open shared memory file */
    fd = open(SHARED_FILE_NAME, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO); 
    if (fd < 0) {
        fprintf (stderr, "failed to open shmem file \n");
    }

    /* rank-0 create shared region  */
    if (ftruncate (fd, bufsize) < 0) {
        fprintf (stderr, "ftruncate failed \n");
    }
    
    /* map the file */
    shmem_ptr = mmap (0, bufsize, PROT_READ | PROT_WRITE, 
                        MAP_SHARED, fd, 0);
    if (shmem_ptr == MAP_FAILED) {
        fprintf (stderr, "mmap shared buffer failed\n");
    }
   
    /* allocate dummybuf memory and set initial values */
    if (rank == sender) {
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
    }

    /* synchronize */
    MPI_Barrier(MPI_COMM_WORLD);
    
    /* == benchmark ==
     * 1. Sender Copies the iov list to shmem region.
     * 2. Bcast the control message to everyone else notifying about it.
     * 3. Receiver(s) copy from the shmem region.
     * 
     */
 
    //start = MPI_Wtime();
    for (i = 0; i < iters+skip; i++) {
        if (i == skip) start = MPI_Wtime();
     
        if (rank == sender) {
            nread = copy_to_shmem(shmem_ptr, liov, blocksize, bufsize);
            assert (nread == bufsize);
#if VALIDATE
            print_buff_region(rank, shmem_ptr, bufsize, 'x');
#endif
        }

        /* RTS Packet: Sender indicates the data is ready to be read from shared memory */
        MPI_Bcast(&dummy, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

        if (rank != sender) {
            nread = copy_from_shmem(shmem_ptr, liov, blocksize, bufsize);
            assert (nread == bufsize);
#if VALIDATE      
            print_buff_region(rank, shmem_ptr, bufsize, 'x');
#endif
        }

        /* FIN Packet: Sender ensures that everyone has copied the data */
        MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD); 
    }
    end = MPI_Wtime();
    lat = (end - start) * 1e6 / iters;
 
    MPI_Barrier(MPI_COMM_WORLD);
    
    if(rank == sender) {
        PRINT (benchmark, blocksize, bufsize, lat);
    }

    /* free buffer */
    if (lbuf) free(lbuf);

    /* destroy iovec list */
    if (liov) free(liov);

    /* destroy (unmap) shmem region */
    if (munmap(shmem_ptr, bufsize) < 0) {
        fprintf (stderr, "munmap on shared buffer failed\n");
    }

    if (dummybuf) {
        free (dummybuf);
    }

    /* close the file handle */
    if (fd != -1) {
        close (fd);
    }
        
    MPI_Finalize();
    
    return 0;
}
