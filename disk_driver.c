#include "disk_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define BYTE_DIM 8

// opens the file (creating it if necessary)
// allocates the necessary space on the disk
// calculates how big the bitmap should be
// if the file was new
// compiles a disk header, and fills in the bitmap of appropriate size
// with all 0 (to denote the free space);
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks)
{
    int res = access(filename, F_OK); //esiste veramente il pathname?
    if(res == -1) handle_error("Pathname error");

    int fd = open(filename, O_CREAT| O_RDWR , 0666);
    if(fd == -1) handle_error("Error opening file, fd is -1"); 

    int bitmap_size = num_blocks / BYTE_DIM;
    //eseguire la mmap
}

/*
   The blocks indices seen by the read/write functions 
   have to be calculated after the space occupied by the bitmap
*/