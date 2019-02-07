#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <xpmem.h>

#define NUM_ITER 100
#define SKIP_ITER 10
#define VALIDATE 0
#define PRINT_DEBUG 0
#define PRINT_INFO 0


/* shared memory params */
#define SHARED_FILE_NAME "/tmp/shmem.file"
#define SHMEM_FSIZE 4194304 /* 4M */
#define SHMEM_PIPELINE_DEPTH 65536 /* pagesize * 16 */

#define PRINT(benchmark,blocksize,bufsize,latency)                                   \
    do {                                                                                \
        fprintf(stdout, "%-8s - Blocksize: %8ld  Bufsize: %8ld  latency(us): %5.4lf\n", \
                benchmark, blocksize, bufsize, latency);                                \
    } while (0)


#define PRINT_HEADER()                                                                          \
    do {                                                                                        \
        fprintf(stdout, "%10s %10s %13s %13s %13s %13s %13s\n", "--------", "--------",         \
                "--------", "--------", "--------", "--------", "--------");                    \
        fprintf(stdout, "%10s %10s %13s %13s %13s %13s %13s\n",                                 \
            "IOV_size", "IOV_count", "SHMEM_NP", "SHMEM_P", "CMA_P", "XPMEM_NC", "XPMEM_C");    \
        fprintf(stdout, "%10s %10s %13s %13s %13s %13s %13s\n", "--------", "--------",         \
                "--------", "--------", "--------", "--------", "--------");                    \
    } while (0)

#define PRINT_LINE(iovlen, iovcnt, shm, shm_p, cma, xpm_nc, xpm_c)                              \
    do {                                                                                        \
        fprintf(stdout, "%10ld %10ld %13.2lf %13.2lf %13.2lf %13.2lf %13.2lf\n",   \
                iovlen, iovcnt, shm, shm_p, cma, xpm_nc, xpm_c);                       \
    } while (0)

extern ssize_t process_vm_readv(pid_t pid, 
        const struct iovec *local_iov,
        unsigned long liovcnt,
        const struct iovec *remote_iov,
        unsigned long riovcnt,
        unsigned long flags);

#if VALIDATE
static inline void print_buff_region (int rank, void *buf, int length, char val) {
    int i, err=0;
    char *msg = (rank == 0) ? "sent_data" : "recv_data";
    for (i = 0; i < length; i++) {
#if PRINT_DEBUG
        printf ("%d: %s [%d] - %c\n", rank, msg, i, *((char *)(buf + i)));
#endif
        if  (*((char *)buf+i) != val) {
            err++;
        }
    }
#if PRINT_INFO
    if (err > 0) {
        fprintf (stderr, "[%d]: %d errors found ...\n", rank, err);
    } else {
        fprintf (stdout, "[%d]: Validation successfull...\n", rank);
    }
#endif
}
#endif


// ==================================================================================== //
#if 0
static inline void print_iovec (int rank, struct iovec *liov, int blocksize, int bufsize) {
    int i, j, nblocks;
    nblocks = bufsize / blocksize;
    for (i = 0; i < nblocks; i++) {
        for (j = 0; j < liov[i].iov_len; j++) {
            printf ("%d: local_data [%d] - %c\n", rank,
                    (i*blocksize+j), *((char *)(liov[i].iov_base + j)));
        }
    }
}

static inline void print_shmem_region (int rank, void *shmem_ptr, int length) {
    int i;
    for (i = 0; i < length; i++) {
        printf ("%d: shmem_data [%d] - %c\n", rank, i, *((char *)(shmem_ptr + i)));
    }
}
#endif
