#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <mpi.h>

#define SKIP 10
#define ITER 100

extern ssize_t process_vm_readv(pid_t pid,
        const struct iovec *local_iov,
        unsigned long liovcnt,
        const struct iovec *remote_iov,
        unsigned long riovcnt,
        unsigned long flags);

extern int gettimeofday(struct timeval *, void *);

static inline double getMicrosecondTimeStamp()
{
    double retval;
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) {
        perror("gettimeofday");
        abort();
    }
    retval = ((double)tv.tv_sec) * 1000000 + tv.tv_usec;
    return retval;
}
#define TIMER() getMicrosecondTimeStamp()


int main(int argc, char **argv)
{
    const long bufsize = 1024 * 1024 * 32; //16 MB
    size_t size, nbytes;
    int rank, nproc;
    char *local_buf;
    struct iovec liov, riov;
    size_t page_size;
    pid_t rpid;
    double start, end, lat;
    int i, j, sender = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    page_size = sysconf(_SC_PAGESIZE);
   
    /* malloc buffers */ 
    local_buf = malloc(bufsize);
    posix_memalign((void *)&local_buf, page_size, bufsize);
    
    if (rank == sender) {
        rpid = getpid();
        memset(local_buf, 0xFFFFFFFF, bufsize);
    } else {
        memset(local_buf, 0x00000000, bufsize);
    }

    /* Broadcast the pid of the sender to peer processes */
    MPI_Bcast(&rpid, sizeof(pid_t), MPI_BYTE, sender, MPI_COMM_WORLD);
    
    /* benchmark */
    if (rank != sender) 
        fprintf(stdout, "%10s %15s\n", "Bufsize", "Latency (us)");

    MPI_Barrier (MPI_COMM_WORLD);
    for (size = 1; size <= bufsize; size<<=1) {
        /* create and Broadcast the sender's IOVs */
        liov.iov_base = local_buf;
        liov.iov_len = size;
        
        if (rank == sender) {
            MPI_Bcast(&liov, sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD); 
        } else {
            MPI_Bcast(&riov, sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD); 

            for (i = 0; i < ITER+SKIP; i++) {
                if (i == SKIP) start = TIMER();
                nbytes = process_vm_readv(rpid, &liov, 1, &riov, 1, 0);
                assert (nbytes == size);
            }
            end = TIMER();
            lat = (end - start) / ITER;
            fprintf(stdout, "%10ld %13.2lf\n", size, lat );
        }
    }
    MPI_Barrier (MPI_COMM_WORLD);

    free(local_buf);

    MPI_Finalize();

    return 0;
}

