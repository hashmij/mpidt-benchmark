#include <mpi.h>
#include <util.h>
#include <unistd.h>       


/* xpmem virtual address range */
#if defined(__x86_64__)
#define MAX_ADDRESS ((unsigned long)0x7ffffffff000ul)
#else
#define MAX_ADDRESS XPMEM_MAXADDR_SIZE
#endif

int main(int argc, char **argv)
{
    char *benchmark = "XPMEM_C";
    int i, j, offset;
    int iters = NUM_ITER;
    int skip = SKIP_ITER;
    int rank, size;
    void *lbuf = NULL;
    void **attached_seglist = NULL;
    struct iovec *iov = NULL;
    size_t blocksize, bufsize, nblocks;
    size_t page_size, page_mask;
    double start, end, lat;
    int sender = 0;
    int use_cache = 1;
    char dummy;
    char *dummybuf = NULL;
    void *pagebase_addr = NULL;
    int pg_off = 0;

    xpmem_segid_t segid;
    struct xpmem_addr addr;
    void *lastpagebase_addr = NULL; 
    void *curr_attach_ptr = NULL;
        
    xpmem_apid_t apid;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    assert(argc == 3);

    blocksize = atoi(argv[1]);
    bufsize   = atoi(argv[2]);
    
    nblocks = bufsize / blocksize;

    assert(blocksize > 0);
    assert(bufsize   > 0);

    if (xpmem_version() < 0) {
        fprintf (stderr, "XPMEM driver is not working on the node\n");
    }
    
    /* sender creates the segid token once and Bcast it -- Init phase */
    if (rank == sender) {
        segid = xpmem_make (0, MAX_ADDRESS, XPMEM_PERMIT_MODE, (void *)0666);   
    }

    /* Broadcast the segid of the sender to peer processes */
    MPI_Bcast(&segid, sizeof(xpmem_segid_t), MPI_BYTE, sender, MPI_COMM_WORLD);
   
    /* create and initialize the data buffer  */  
    lbuf = malloc(bufsize);
    page_size = sysconf(_SC_PAGESIZE);
    page_mask = page_size - 1;
    posix_memalign(&lbuf, page_size, bufsize);

    if (rank == sender) {
        memset(lbuf, 'x', bufsize);
    } else {
        memset(lbuf, 0, bufsize);
    }

    /* create and initialize iovec list */
    iov = malloc(sizeof(struct iovec) * nblocks);
    for (i = 0; i < nblocks; i++) {
        iov[i].iov_base = lbuf + i * blocksize;
        iov[i].iov_len = blocksize;
    }


    /* create list of pointers that'll hold remote attached addresses in local
     * virtual address range */ 
    attached_seglist = malloc (sizeof(void *) * nblocks);   
    memset(attached_seglist, 0, nblocks * sizeof (void *));
   
    /* allocate dummybuf memory and set initial values */
    if (rank == sender) {
        dummybuf = malloc(size);
        memset (dummybuf, 0, size);
    }
 
    /* synchronize */ 
    MPI_Barrier(MPI_COMM_WORLD);

    /* == begin benchmark == */
   
    for (j = 0; j < iters+skip; j++) {
        if (j == skip) start = MPI_Wtime();

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
                    if (iov[i].iov_len < page_size) {
                        pagebase_addr = (void *)((unsigned long)iov[i].iov_base & ~page_mask);
                        if (lastpagebase_addr != pagebase_addr) {
                            lastpagebase_addr = pagebase_addr;
                            addr.offset = (uintptr_t)pagebase_addr;
                            curr_attach_ptr = xpmem_attach(addr, page_size, NULL);
                            if (curr_attach_ptr == (void *)-1) {
                                fprintf (stderr, "faild to attach in %d iteration \n", i);
                                goto fn_exit;
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
                if (iov[i].iov_len < page_size) {
                    pagebase_addr = (void *) ((unsigned long)iov[i].iov_base & ~page_mask);
                    pg_off = ((unsigned long)iov[i].iov_base - (unsigned long)pagebase_addr);
                } else {
                    pg_off = 0;
                }
                memcpy (lbuf+offset, attached_seglist[i]+pg_off, iov[i].iov_len);
                offset += iov[i].iov_len;
            }
            
            /* 4. detach and release mapped pages only when use_cache = 0 or
             * last iteration */
            if (!use_cache || (j+1 == iters+skip)) {
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
    /* end timer */
    end = MPI_Wtime();
    
    lat = (end - start) * 1e6 / iters;
   
fn_exit: 
    if(rank == sender) {
        PRINT (benchmark, blocksize, bufsize, lat);
    }
    
    
    if (0 != apid) {
        xpmem_release(apid);
    }

    if (attached_seglist) {
        free (attached_seglist);
    }
    
    /* free buffer */
    if (lbuf != NULL) { 
        free(lbuf);
    }
    if (dummybuf) {
        free (dummybuf);
    }

    /* destroy iovec list */
    if (iov) {
        free(iov);
    }
        
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    
    return 0;
}
