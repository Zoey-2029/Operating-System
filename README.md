## PintOS Operating System ([Stanford CS140](https://www.scs.stanford.edu/21wi-cs140/pintos/pintos_1.html#SEC2))

### Threads ([project 1](https://www.scs.stanford.edu/21wi-cs140/pintos/pintos_2.html#SEC15))
* Implemented **priority scheduling**, solved the problem of "priority inversion" through priority donation 
* Implemented a **multilevel-feedback queue scheduler** similar to the BSD scheduler to reduce the average response time for running jobs on the system.

### User Programs ([project 2](https://www.scs.stanford.edu/21wi-cs140/pintos/pintos_3.html#SEC32))

* Parsed filename and arguments for user program, set up stack to support **argument passing**.
* Implemented the support for user programs to request **system call** functions (kernel functions).

### Virtual Memory ([project 3](https://www.scs.stanford.edu/21wi-cs140/pintos/pintos_4.html#SEC53))
* Designed and implemented **Supplemental page table, Frame table, Swap table and Table of file mappings**
* Supported **stack growth**, allwed user stack to allocate additional pages asnecessary
* Impelemented **File Memory Mapping**, made open files accessible via direct memory access.
* Supported **swapping**, incluidng "swapping in"(allocate a new frame and move it to memory when page fault handler finds a page is not memory but
in swap disk) and "swapping out"(evict a page from its frame and put a copy of into swap disk when out of free frames) 

### File Systems ([project 4](https://www.scs.stanford.edu/21wi-cs140/pintos/pintos_5.html#SEC75))
* Supported **file extension** by developing an index structure with direct, indirect, and doubly indirect blocks. 
* Developed a **hierarchical name space**. In the basic file system, all files live in a single directory. Updated it to allow directory entries to point to files or to other directories.
* Updated file system to support **Buffer Cache**. When a request is made to read or write a block, check to see if it is in the cache, and if so, use the cached data without going to disk. Otherwise, fetch the block from disk into the cache, evicting an older entry if necessary. And the implementation guarantees to be "write-behind" and "read-ahead". 
