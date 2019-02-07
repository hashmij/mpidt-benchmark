/*
 * author: Jahanzeb Hashmi
 * hashmi.29@osu.edu
 *
 * Network Based Computing Laboratory (NBCL)
 * The Ohio State Universityi
 *
 */

#include <mpi.h>
#include <util.h>

#define CMA_IOV_LIMIT sysconf(_SC_IOV_MAX)
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#define PAGE_MASK (PAGE_SIZE-1)

static inline int init_shmem(int *fd, void **shmem_ptr, size_t bufsize) {
    /* open shared memory file */
    *fd = open(SHARED_FILE_NAME, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (*fd < 0) {
        fprintf (stderr, "failed to open shmem file \n");
        return -1;
    }

    /* rank-0 create shared region  */
    if (ftruncate (*fd, bufsize) < 0) {
        fprintf (stderr, "ftruncate failed \n");
        return -2;
    }

    /* map the file */
    *shmem_ptr = mmap (0, bufsize, PROT_READ | PROT_WRITE,
                        MAP_SHARED, *fd, 0);
    if (*shmem_ptr == MAP_FAILED) {
        fprintf (stderr, "mmap shared buffer failed\n");
        return -3;
    }
    
    return 0;
}


static inline int cleanup_shmem(void **shmem_ptr, size_t bufsize, int *fd) {
    
    /* destroy (unmap) shmem region */
    if (munmap(*shmem_ptr, bufsize) < 0) {
        fprintf (stderr, "munmap on shared buffer failed\n");
    }

    /* close the file handle */
    if (*fd != -1) {
        close (*fd);
    }
    return 0;
}

static inline int copy_to_shmem(void *shmem_ptr,
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

static inline int copy_from_shmem(void *shmem_ptr,
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

double benchmark_shmem_pip(int rank, int size, int sender, void *shmem_ptr, 
        void *lbuf, struct iovec *liov, size_t bufsize, size_t blocksize) 
{
    double start, end, lat;
    char dummy;
    char *dummybuf = NULL;
    int j;
    int iovcnt, niov, shm_off, iov_off;
    size_t capacity, full, len;
    size_t nread;

     /* allocate dummybuf memory and set initial values */
    if (rank == sender) {
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }


    full = SHMEM_PIPELINE_DEPTH; /* defined in util */

    for (j = 0; j < NUM_ITER+SKIP_ITER; j++) {

        if (j == SKIP_ITER)  start = MPI_Wtime();

        iovcnt = iov_off = shm_off = nread = 0;
        capacity = full;
        niov = bufsize / blocksize;

        if (rank == sender) {
            while (iovcnt < niov) {
                capacity = full;
                while (capacity > 0) {
                    if (capacity >= (liov[iovcnt].iov_len - iov_off)) {
                        len = liov[iovcnt].iov_len - iov_off;
                        memcpy (shmem_ptr + shm_off,
                                liov[iovcnt].iov_base + iov_off,
                                len);
                        nread += len;
                        shm_off += len;
                        iovcnt ++;
                        capacity -= len;
                        iov_off = 0;
                    } else {
                        len = capacity;
                        memcpy (shmem_ptr + shm_off,
                                liov[iovcnt].iov_base + iov_off,
                                len);
                        nread += len;
                        iov_off += len;
                        shm_off += len;
                        capacity -= len; // capacity should be zero after this
                    }
                }

                MPI_Bcast(&dummy, 1, MPI_BYTE, sender, MPI_COMM_WORLD);
            }

            /* Final Gather to ensure all the iov's have been copied */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

            assert (iovcnt == niov);
            assert (nread == bufsize);
#if VALIDATE
            print_buff_region(rank, shmem_ptr, bufsize, 'x');
#endif
        }

        if (rank != sender) {
            while (iovcnt < niov) {
                MPI_Bcast(&dummy, 1, MPI_BYTE, sender, MPI_COMM_WORLD);
                capacity = full;
                while (capacity > 0) {
                    if (capacity >= (liov[iovcnt].iov_len - iov_off)) {
                        len = liov[iovcnt].iov_len - iov_off;
                        memcpy (liov[iovcnt].iov_base + iov_off,
                                shmem_ptr + shm_off,
                                len);
                        nread += len;
                        shm_off += len;
                        iovcnt ++;
                        capacity -= len;
                        iov_off = 0;
                    } else {
                        len = capacity;
                        memcpy (liov[iovcnt].iov_base + iov_off,
                                shmem_ptr + shm_off,
                                len);
                        nread += len;
                        iov_off += len;
                        shm_off += len;
                        capacity -= len; // capacity should be zero after this
                    }
                }
            }

            /* Final Gather to ensure all the iov's have been copied */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

            assert (iovcnt == niov);
            assert (nread == bufsize);
#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        }
    }
    end = MPI_Wtime();
    lat = (end - start) * 1e6 / NUM_ITER;

    MPI_Barrier(MPI_COMM_WORLD);
    
    /* free up dummy buffer */
    if (dummybuf) {
        free (dummybuf);
    }

    return lat;
}



double benchmark_shmem(int rank, int size, int sender, void *shmem_ptr, 
        void *lbuf, struct iovec *liov, size_t bufsize, size_t blocksize) {
    
    double start, end, lat;
    size_t nread;
    char dummy;
    char *dummybuf = NULL;
    int i;

    /* allocate dummybuf memory and set initial values */
    if (rank == sender) {
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }

    /* initialize data */
    for (i = 0; i < NUM_ITER + SKIP_ITER; i++) {
        if (i == SKIP_ITER) start = MPI_Wtime();

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
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        }

        /* FIN Packet: Sender ensures that everyone has copied the data */
        MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);
    }
    end = MPI_Wtime();
    lat = (end - start) * 1e6 / NUM_ITER;

    MPI_Barrier(MPI_COMM_WORLD);
    
    /* free up dummy buffer */
    if (dummybuf) {
        free (dummybuf);
    }

    return lat;
}


double benchmark_cma (int rank, int size, int sender, struct iovec *riov, struct iovec *liov, 
        void *lbuf, size_t bufsize, size_t blocksize) {

    double start, end, lat;
    char dummy;
    char *dummybuf = NULL;
    int j, nblocks, iov_off;
    size_t bytes_copied = 0, remaining = 0, total_copied = 0;
    pid_t rpid;
        
    nblocks = bufsize / blocksize;
    
    if (rank == sender) {
        rpid = getpid();
    }

    /* Broadcast the pid of the sender to peer processes */
    MPI_Bcast(&rpid, sizeof(pid_t), MPI_BYTE, sender, MPI_COMM_WORLD);

     /* allocate dummybuf memory and set initial values */
    if (rank == sender) {
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }

    for (j = 0; j < NUM_ITER+SKIP_ITER; j++) {

        if (j == SKIP_ITER) start = MPI_Wtime();

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
    lat = (end - start) * 1e6 / NUM_ITER;
  
    MPI_Barrier(MPI_COMM_WORLD);
 
    /* free up dummy buffer */
    if (dummybuf) {
        free (dummybuf);
    } 
    return lat;
}

double benchmark_xpmem_nocache (int rank, int size, int sender, struct iovec *iov, 
        void *lbuf, size_t bufsize, size_t blocksize) {

    double start, end, lat;
    char dummy;
    void **attached_seglist = NULL;
    char *dummybuf = NULL;
    int i, j, nblocks, offset, pg_off = 0;

    xpmem_segid_t segid;
    struct xpmem_addr addr;
    xpmem_apid_t apid;
    void *pagebase_addr = NULL;
    void *lastpagebase_addr = NULL;
    void *curr_attach_ptr = NULL;

    nblocks = bufsize / blocksize;
    
    if (xpmem_version() < 0) {
        fprintf (stderr, "XPMEM driver is not working on the node\n");
        return 0;
    }

   /* sender creates the segid token once and Bcast it -- Init phase */
    if (rank == sender) {
        segid = xpmem_make (0, XPMEM_MAXADDR_SIZE, XPMEM_PERMIT_MODE, (void *)0666);
        /* allocate dummybuf memory and set initial values */
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }

    /* Broadcast the segid of the sender to peer processes */
    MPI_Bcast(&segid, sizeof(xpmem_segid_t), MPI_BYTE, sender, MPI_COMM_WORLD);

    /* create list of pointers that'll hold remote attached addresses in local
     * virtual address range */
    attached_seglist = malloc (sizeof(void *) * nblocks);
    memset(attached_seglist, 'x', nblocks * sizeof (void *));
    

    for (j = 0; j < NUM_ITER+SKIP_ITER; j++) {
        if (j == SKIP_ITER) start = MPI_Wtime();

        if (rank == sender) {
            /* 1. Send RTS: Broadcast the list of iov structs to peer processes */
            MPI_Bcast(iov, nblocks*sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD);

            /* 2. Wait for FIN: Sender ensures that peers have copied the data  */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        } else {

            /* 1. Recv RTS: recv sender's iov struct into local iov struct */
            MPI_Bcast(iov, nblocks*sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD);

            /* 2. Attach to remote data segments */
            apid = xpmem_get (segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, (void*)0666);
            addr.apid = apid;
            for (i = 0; i < nblocks; i++) {
                if (iov[i].iov_len < PAGE_SIZE) {
                    pagebase_addr = (void *)((unsigned long)iov[i].iov_base & ~PAGE_MASK);

                    if (lastpagebase_addr != pagebase_addr) {
                        lastpagebase_addr = pagebase_addr;
                        addr.offset = (uintptr_t)pagebase_addr;
                        curr_attach_ptr = xpmem_attach(addr, PAGE_SIZE, NULL);
                        if (curr_attach_ptr == (void *)-1) {
                            fprintf (stderr, "faild to attach in %d iteration \n", i);
                            goto cleanup;
                        }
                    }
                    attached_seglist[i] = curr_attach_ptr;
                } else {
                    addr.offset = (uintptr_t) iov[i].iov_base;
                    attached_seglist[i] = xpmem_attach(addr, iov[i].iov_len, NULL);
                    if (attached_seglist[i] == (void *)-1) {
                        fprintf (stderr, "faild to attach in %d iteration \n", i);
                    }
                }
            }

            /* 3. Copy from remote segments */
            offset = 0;
            for (i = 0; i < nblocks; i++) {
                if (iov[i].iov_len < PAGE_SIZE) {
                    pagebase_addr = (void *) ((unsigned long)iov[i].iov_base & ~PAGE_MASK);
                    pg_off = ((uintptr_t)iov[i].iov_base - (uintptr_t)pagebase_addr);
                } else {
                    pg_off = 0;
                }
                memcpy (lbuf + offset, attached_seglist[i]+pg_off, iov[i].iov_len);
                offset += iov[i].iov_len;
            }

            /* 4. detach and release mapped pages */
            for (i = 0; i < nblocks; i++) {
                if (NULL != attached_seglist[i]) {
                    xpmem_detach(attached_seglist[i]);
                }
            }

            /* 5. Send FIN pkt  */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);
#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        }
    }

    end = MPI_Wtime();
    lat = (end - start) * 1e6 / NUM_ITER;

    MPI_Barrier(MPI_COMM_WORLD);

cleanup:
    if (0 != apid) xpmem_release(apid);
    if (attached_seglist) free (attached_seglist);
    if (dummybuf) free (dummybuf);

    return lat;

}



double benchmark_xpmem_cached(int rank, int size, int sender, struct iovec *iov, 
        void *lbuf, size_t bufsize, size_t blocksize) {

    int use_cache = 1;
    double start, end, lat;
    char dummy;
    void **attached_seglist = NULL;
    char *dummybuf = NULL;
    int i, j, nblocks, offset, pg_off = 0;

    xpmem_segid_t segid;
    struct xpmem_addr addr;
    xpmem_apid_t apid;
    void *pagebase_addr = NULL;
    void *lastpagebase_addr = NULL;
    void *curr_attach_ptr = NULL;

    nblocks = bufsize / blocksize;
    
    if (xpmem_version() < 0) {
        fprintf (stderr, "XPMEM driver is not working on the node\n");
        return 0;
    }

   /* sender creates the segid token once and Bcast it -- Init phase */
    if (rank == sender) {
        segid = xpmem_make (0, XPMEM_MAXADDR_SIZE, XPMEM_PERMIT_MODE, (void *)0666);
        /* allocate dummybuf memory and set initial values */
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }

    /* Broadcast the segid of the sender to peer processes */
    MPI_Bcast(&segid, sizeof(xpmem_segid_t), MPI_BYTE, sender, MPI_COMM_WORLD);

    /* create list of pointers that'll hold remote attached addresses in local
     * virtual address range */
    attached_seglist = malloc (sizeof(void *) * nblocks);
    memset(attached_seglist, 'x', nblocks * sizeof (void *));
   
    /* benchmark loop */ 

    for (j = 0; j < NUM_ITER+SKIP_ITER; j++) {
        if (j == SKIP_ITER) start = MPI_Wtime();

        if (rank == sender) {
            /* 1. Send RTS: Broadcast the list of iov structs to peer processes */
            MPI_Bcast(iov, nblocks*sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD);

            /* 2. Wait for FIN: Sender ensures that peers have copied the data  */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        } else {

            /* 1. Recv RTS: recv sender's iov struct into local iov struct */
            MPI_Bcast(iov, nblocks*sizeof(struct iovec), MPI_BYTE, sender, MPI_COMM_WORLD);

            /* 2. Attach to remote data segments. Attach only once if use_cache = 1 */
            if (use_cache && j == 0) {
                apid = xpmem_get (segid, XPMEM_RDWR, XPMEM_PERMIT_MODE, (void*)0666);
                addr.apid = apid;
                for (i = 0; i < nblocks; i++) {
                    if (iov[i].iov_len < PAGE_SIZE) {
                        pagebase_addr = (void *)((unsigned long)iov[i].iov_base & ~PAGE_MASK);
                        if (lastpagebase_addr != pagebase_addr) {
                            lastpagebase_addr = pagebase_addr;
                            addr.offset = (uintptr_t)pagebase_addr;
                            curr_attach_ptr = xpmem_attach(addr, PAGE_SIZE, NULL);
                            if (curr_attach_ptr == (void *)-1) {
                                fprintf (stderr, "faild to attach in %d iteration \n", i);
                                goto cleanup;
                            }
                        }
                        attached_seglist[i] = curr_attach_ptr;
                    } else {
                        addr.offset = (uintptr_t)iov[i].iov_base;
                        attached_seglist[i] = xpmem_attach(addr, iov[i].iov_len, NULL);
                        if (attached_seglist[i] == (void *)-1) {
                            fprintf (stderr, "faild to attach in %d iteration \n", i);
                        }
                    }
                }
            }
            /* 3. Copy from remote segments */
            offset = 0;
            for (i = 0; i < nblocks; i++) {
                if (iov[i].iov_len < PAGE_SIZE) {
                    pagebase_addr = (void *) ((unsigned long)iov[i].iov_base & ~PAGE_MASK);
                    pg_off = ((unsigned long)iov[i].iov_base - (unsigned long)pagebase_addr);
                } else {
                    pg_off = 0;
                }
                memcpy (lbuf+offset, attached_seglist[i]+pg_off, iov[i].iov_len);
                offset += iov[i].iov_len;
            }

            /* 4. detach and release mapped pages only when use_cache = 0 or
             * last iteration */
            if (!use_cache || (j+1 == NUM_ITER+SKIP_ITER)) {
                for (i = 0; i < nblocks; i++) {
                    if (NULL != attached_seglist[i]) {
                        xpmem_detach(attached_seglist[i]);
                    }
                }
            }
            /* 5. Send FIN pkt  */
            MPI_Gather(&dummy, 1, MPI_BYTE, dummybuf, 1, MPI_BYTE, sender, MPI_COMM_WORLD);

#if VALIDATE
            print_buff_region(rank, lbuf, bufsize, 'x');
#endif
        }
    }

    end = MPI_Wtime();
    lat = (end - start) * 1e6 / NUM_ITER;

    MPI_Barrier(MPI_COMM_WORLD);

cleanup:
    if (0 != apid) xpmem_release(apid);
    if (attached_seglist) free (attached_seglist);
    if (dummybuf) free (dummybuf);

    return lat;

}



int main(int argc, char **argv)
{
    int i;
    int rank, size;
    void *lbuf = NULL;
    void *shmem_ptr = NULL;
    struct iovec *liov = NULL, *riov = NULL;
    size_t bufsize, nblocks, min_bs, max_bs;
    size_t blocksize = 4; /* default: otherwise min_bs */
    double shmem_lat, shmem_p_lat, cma_lat, xpm_nc_lat, xpm_c_lat;
    int sender = 0;
    int fd = -1;
    

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 4) {
        fprintf(stderr, "usage: ./prog <buffer_size> <min_block_size> <max_block_size>\n");
        goto fn_exit;
    }

    bufsize  = atoi(argv[1]); /* max buffer size */ 
    min_bs   = atoi(argv[2]); /* min block sie */
    max_bs   = atoi(argv[3]); /* max block size */
 
    assert(bufsize   > 0);
    assert(min_bs <= max_bs && max_bs <= bufsize && max_bs > 0 && min_bs > 0);

    lbuf = malloc(bufsize);
    posix_memalign(&lbuf, PAGE_SIZE, bufsize);
    
    /* initial synchronize */
    MPI_Barrier(MPI_COMM_WORLD);

    
    if (rank == sender) {
        printf ("buffer size: %ld bytes\n", bufsize);
        PRINT_HEADER();
    }

    /*+*+*+*+*+*+*+*+*+*+*+*+* Beging Benchmarking +*+*+*+*+*+*+*+*+*+*+*+*+*/

    /* start from min block size and left shit 1 in each iteration until max_bs
     * */
    blocksize = min_bs;

    while (blocksize <= max_bs) {
        nblocks = bufsize / blocksize;
        assert(blocksize > 0);
       
        /* create and init iovec list */
        liov = malloc(sizeof(struct iovec) * nblocks);
        riov = malloc(sizeof(struct iovec) * nblocks);

        /* initialize iovec list */
        for (i = 0; i < nblocks; i++) {
            liov[i].iov_base = lbuf + i * blocksize;
            liov[i].iov_len = blocksize;
        }
        
        /* benchmark - SHMEM + SHMEM_PIPELINE */
        init_shmem (&fd, &shmem_ptr, bufsize);
        shmem_lat = benchmark_shmem(rank, size, sender, shmem_ptr, lbuf, liov, bufsize, blocksize);
        
        MPI_Barrier(MPI_COMM_WORLD);

        shmem_p_lat = benchmark_shmem_pip(rank, size, sender, shmem_ptr, lbuf, liov, 
                bufsize, blocksize);

        cleanup_shmem (&shmem_ptr, bufsize, &fd);

        MPI_Barrier(MPI_COMM_WORLD);
        
        /* benchmark - CMA */
        cma_lat = benchmark_cma (rank, size, sender, riov, liov, lbuf, bufsize, blocksize);

        MPI_Barrier(MPI_COMM_WORLD);
      
        /* benchmark - XPMEM (No Caching) */ 
        xpm_nc_lat = benchmark_xpmem_nocache (rank, size, sender, liov, lbuf, bufsize, blocksize); 
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        xpm_c_lat = benchmark_xpmem_cached(rank, size, sender, liov, lbuf, bufsize, blocksize); 
        
        /* final synchronization */
        MPI_Barrier(MPI_COMM_WORLD);
        
        if (rank == 0) {
            PRINT_LINE(blocksize, nblocks, shmem_lat, shmem_p_lat, cma_lat, xpm_nc_lat, xpm_c_lat);
        }
        
        /* update block size for next iteration */
        blocksize = (blocksize << 1);
        
        /* destroy iovec list */
        if (liov) free(liov);
        if (riov) free(riov);
    }
    
fn_exit:

    /* free buffer */
    if (lbuf) free(lbuf);

    MPI_Finalize();

    return 0;
}






