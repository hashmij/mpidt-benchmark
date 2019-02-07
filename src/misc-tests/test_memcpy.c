#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define SKIP 10
#define ITER 100

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
    size_t size;
    char *sbuf, *rbuf;
    size_t page_size;
    double start, end, lat;
    int i, j;

    page_size = sysconf(_SC_PAGESIZE);
    
    sbuf = malloc(bufsize);
    posix_memalign((void *)&sbuf, page_size, bufsize);
    memset(sbuf, 0xFFFFFFFF, bufsize);
    
    rbuf = malloc(bufsize);
    posix_memalign((void *)&rbuf, page_size, bufsize);
    memset(rbuf, 0x00000000, bufsize);

        
    fprintf(stdout, "%10s %15s\n", "Bufsize", "Latency (us)");

    for (size = 1; size <= bufsize; size<<=1) {
        for (i = 0; i < ITER+SKIP; i++) {
            if (i == SKIP) start = TIMER();
            memcpy (rbuf, sbuf, size);
        }
        end = TIMER();
        lat = (end - start) / ITER;
        fprintf(stdout, "%10ld %13.2lf\n", size, lat );
        //printf ("bufsize: %lu, latency(us): %.2f\n", size, lat);
    }

    return 0;
}

