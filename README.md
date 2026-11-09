# Repo for CSCE611 Machine Problems - Aaryan Upadhyaya (UIN:633002888)

## MP2 - Frame Manager

Files changed :
- MP2/cont_frame_pool.C
- MP2/cont_frame_pool.H

Tests done:
- Basic - Kernel pool implementation
- Multiple pools implemented

## MP3 - Page Table Management

Files changed :
- MP3/page_table.C
- MP3/page_table.H
- MP3/cont_frame_pool.C
- MP3/cont_frame_pool.H

Tests done:
- Basic - Set up pools, set up page table, turn paging on, access 256K continuous memory entries in logical address space
- Test page directory updates : In the read-write test, set the address jumps in test loop to 4 MB and check if a new page directory entry is added everytime.

## MP4 - Virtual memory management

Files changed :
- MP4/page_table.H
- MP4/page_table.C
- MP4/vm_pool.H
- MP4/vm_pool.C
- MP4/console.H
- MP4/console.C - for debugging, the Console::putui function can print 32 characters now
- MP4/utils.H
- MP4/utils.C - support functions for console.C

Tests done:
- Recursive page table lookup
- Virtual memory pools implemented

## MP5 - Kernel-level thread scheduling

Files changed :
- scheduler.H
- scheduler.C
- threads.H
- threads.C
- eoq_timer.H
- eoq_timer.C
- makefile - added command-line options to enable different tests
- kernel.C - added macros to compile features selectively

Tests done:
- FIFO scheduling without pre-emption
- Pre-emption by a timer
- Cleaning terminating functions
- Round-robin scheduling through pre-emption