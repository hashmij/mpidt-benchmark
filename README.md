1. Summary
-----------
MPI datatype benchmarks using various intra-node communication mechanisms i.e., 
POSIX SHMEM, XPMEM, and CMA, etc. The benchmarks support single and multi-pair 
configuration  where in multi-pair configuration `sender' process owns and 
initiates the data movement while receiver peers copy / receive the data in 
their local buffers.

2. Compiling
-------------
Update makefile with the MPI and XPMEM install directories and do:
$ make

3. Running 
-----------

Use MPI to launch the benchmarks. If you wish to launch individual benchmark to
see the performance of each individual copying mechanism, say CMA, then 
you can use the following command:

```
$ mpirun_rsh -np 2 -hostfile ~/hosts ./bin/cma_dt.x <blocksize> <bufsize>
```

* Examples:

```
[hashmij@head dt-bench]$ mpirun_rsh -np 28 -hostfile ~/hosts shmem_dt 8192 4194304
method: SHMEM, blocksize: 8192, bufsize: 4194304, latency: 3346.4432 us

[hashmij@head dt-bench]$ mpirun_rsh -np 28 -hostfile ~/hosts xpmem_dt 8192 4194304
method: XPMEM, blocksize: 8192, bufsize: 4194304, latency: 1886.3678 us

[hashmij@head dt-bench]$  mpirun_rsh -np 28 -hostfile ~/hosts cma_dt 8192 4194304
method: CMA, blocksize: 8192, bufsize: 4194304, latency: 10453.4400 us
```


**UPDATE [09-27-2018]** 

I have added a unified benchmark that tests all the copying mechanisms 
as one benchmark and reports the output comparison of all the approaches for 
a given buffer size and range of block sizes. If you wish to compare the 
performance of all the mechanism as one test, then you can use:

```
$ mpirun_rsh -np 2 -hostfile ~/hosts ./bin/bench.x <bufsize> <min_blocksize> <max_blocksize>

```

* Examples:

```
$ mpirun_rsh -np 2 -hostfile ~/hosts MV2_DEBUG_CORESIZE=unlimited ./bin/bench.x 4194304 4 4194304
  
  buffer size: 4194304 bytes
  
  --------   --------      --------      --------      --------      --------      --------
  IOV_size  IOV_count      SHMEM_NP       SHMEM_P         CMA_P      XPMEM_NC       XPMEM_C
  --------   --------      --------      --------      --------      --------      --------
         4    1048576      10667.18       6863.61     209904.12     388168.45      22165.63
         8     524288       5308.75       3431.22     104174.91     194768.74      10868.07
        16     262144       2689.65       1737.02      52266.85      99443.24       5448.51
        32     131072       1538.42        980.95      26294.46      51568.51       2742.09
        64      65536       1254.58        727.71      13103.32      27540.15       1397.51
       128      32768        952.74        566.32       6739.46      15617.32        743.03
       256      16384        777.59        478.75       3407.93       9788.20        525.64
       512       8192        759.57        463.22       1790.75       6917.07        418.39
      1024       4096        768.68        456.66        988.69       5510.66        374.77
      2048       2048        727.44        425.14        635.87       4846.71        345.08
      4096       1024        712.18        413.67        454.70       4564.04        336.59
      8192        512        705.92        409.17        424.44       3056.34        324.46
     16384        256        706.72        407.15        410.92       2255.09        326.26
     32768        128        702.95        405.93        403.93       1841.40        317.81
     65536         64        702.23        402.87        397.06       1622.25        315.85
    131072         32        963.56        403.65        395.21       1560.35        347.32
    262144         16        962.51        405.54        394.07       1503.43        347.83
    524288          8        963.25        404.05        396.10       1474.42        345.83
   1048576          4        934.39        403.43        394.86       1528.49        331.99
   2097152          2        899.37        403.34        393.87       1562.45        318.71
   4194304          1        879.96        403.28        392.09       1573.52        313.02

```

** it is highly recommended to use unified benchmark for evaluation purposes **

--------------------------
Contact: Jahanazeb Hashmi

jahanzeb [dot] maqbool [at] gmail.com
