#pragma once
#include "bitmap.h"

#define BLOCK_SIZE 512
#define BYTE_DIM 8
// this is stored in the 1st block of the disk
typedef struct {
  int num_blocks;
  int bitmap_blocks;   // how many blocks in the bitmap
  int bitmap_entries;  // how many bytes are needed to store the bitmap
  
  int free_blocks;     // free blocks
  int first_free_block;// first block index
} DiskHeader; 

typedef struct {
  DiskHeader* header; // mmapped
  char* bitmap_data;  // mmapped (bitmap)
  int fd; // for us
} DiskDriver;

/**
   The blocks indices seen by the read/write functions 
   have to be calculated after the space occupied by the bitmap
*/

// opens the file (creating it if necessary)
// allocates the necessary space on the disk
// calculates how big the bitmap should be
// if the file was new
// compiles a disk header, and fills in the bitmap of appropriate size with all 0 (to denote the free space);
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks);

// reads the block in position block_num
// returns -1 if the block is free accrding to the bitmap
// 0 otherwise
int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num);

// writes a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num);

// frees a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_freeBlock(DiskDriver* disk, int block_num);

// returns the first free blockin the disk from position (checking the bitmap)
int DiskDriver_getFreeBlock(DiskDriver* disk, int start);

// writes the data (flushing the mmaps)
/*
The program maps a region of memory using mmap. It then modifies the mapped region. 
The system isn't required to write those modifications back to the underlying file immediately, so a read call on that file (in ioutil.ReadAll) could return the prior contents of the file.
The system will write the changes to the file at some point after you make the changes. 
It is allowed to write the changes to the file any time after the changes are made, but by default makes no guarantees about when it writes those changes. 
All you know is that (unless the system crashes), the changes will be written at some point in the future.
If you need to guarantee that the changes have been written to the file at some point in time, then you must call msync.
The mmap.Flush function calls msync with the MS_SYNC flag. 
When that system call returns, the system has written the modifications to the underlying file, so that any subsequent call to read will read the modified file.
*/

int DiskDriver_flush(DiskDriver* disk);

//initialize header
DiskHeader* DiskDriver_initialize_header(DiskHeader* disk_header, int fd, size_t size);

void DiskDriver_print(DiskDriver* disk);