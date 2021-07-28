#include "disk_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h> 

#define BYTE_DIM 8

// opens the file (creating it if necessary)
// allocates the necessary space on the disk
// calculates how big the bitmap should be
// if the file was new
// compiles a disk header, and fills in the bitmap of appropriate size
// with all 0 (to denote the free space);
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks)
{
    if(num_blocks < 0 || disk == NULL) handle_error_en("Invalid param", EINVAL);

    int fd;

    int bitmap_size = (num_blocks / BYTE_DIM) + 1;

    size_t header_size = sizeof(DiskHeader) + bitmap_size;
    //size_t header_size = sizeof(DiskHeader) + bitmap_size + num_blocks*BLOCK_SIZE;


    if(access(filename, F_OK) == 0)
    {   //apertura file
        fd = open(filename, O_RDWR , 0666);
        if(fd == -1) handle_error("Error opening file, fd is -1"); 
        //ensuring disk space
        if(posix_fallocate(fd, 0, header_size)) 
        {
            close(fd);
            handle_error("Error f-allocating");
        }
        

        DiskHeader* hdr = (DiskHeader*) mmap(0, header_size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
        if(hdr == MAP_FAILED)
        {   
            close(fd);
            handle_error("Error while mapping");
        } 
        //prendiamo il primo blocco libero, l'indice
        //hdr->first_free_block = DiskDriver_getFreeBlock(disk, 0);

        disk->header = hdr;
        disk->bitmap_data = (char*) hdr + sizeof(DiskHeader); //puntatore alla mappa, skippo header
        disk->fd = fd;
    }
    else
    {
        fd = open(filename, O_CREAT | O_RDWR | O_TRUNC , 0666);
        if(fd == -1) handle_error("Error opening file, fd is -1"); 

        if(posix_fallocate(fd, 0, header_size))  
        {
            close(fd);
            handle_error("Error f-allocating");
        }

        DiskHeader* hdr = (DiskHeader*) mmap(0, header_size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
        if(hdr == MAP_FAILED)
        {   
            close(fd);
            handle_error("Error while mapping");
        } 
        hdr->num_blocks = num_blocks;
        hdr->bitmap_blocks = bitmap_size;
        hdr->bitmap_entries = num_blocks;
        hdr->free_blocks = num_blocks;
        hdr->first_free_block = 0;

        disk->header = hdr;
        disk->bitmap_data = (char*) hdr + sizeof(DiskHeader);
        disk->fd = fd;

        memset(disk->bitmap_data, 0, bitmap_size); //bit a zero
    }

    printf("Disk driver initialized, proceding.\n");
    printf("Disk driver initialized, proceding..\n");
    printf("Disk driver initialized, proceding...\n");

}

/*
   The blocks indices seen by the read/write functions 
   have to be calculated after the space occupied by the bitmap
*/


// reads the block in position block_num
// returns -1 if the block is free according to the bitmap
// 0 otherwise
int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num)
{   
    if(block_num < 0 || block_num >= disk->header->num_blocks || dest == NULL || disk == NULL) handle_error_en("Invalid param", EINVAL);

    BitMap bm;
    bm.num_bits = disk->header->bitmap_blocks;
    bm.entries = disk->bitmap_data;

    if(BitMap_get(&bm, block_num, 0) == block_num) return -1; //blocco libero

    //Dove iniziare lettura? stesso principio di indexToBlock, ricordandoci di skippare l'header
    int fd = disk->fd;
    int pos = sizeof(DiskHeader) + disk->header->bitmap_entries + (block_num*BLOCK_SIZE);
    //mi sposto all'offset e leggo byte per byte un blocco

    off_t offset = lseek(fd, pos, SEEK_SET);
    if(offset == -1) handle_error("Invalid offset");

    int ret, bytes_reads = 0;
    
	while(bytes_reads < BLOCK_SIZE){
		
        // save bytes_read in dest location
		ret = read(fd, dest + bytes_reads, BLOCK_SIZE - bytes_reads);

		if (ret == -1 && errno == EINTR) continue;
		if(ret == 0) break;
		
		bytes_reads +=ret;

	}

    return 0;
}
