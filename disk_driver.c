#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h> 

#include "disk_driver.h"

DiskHeader* DiskDriver_initialize_header(DiskHeader* disk_header, int fd, size_t size)
{
    disk_header = (DiskHeader*) mmap(0, size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
    // allocates the necessary space on the disk
    if(disk_header == MAP_FAILED)
    {   
        close(fd);
        printf("Error while mapping, returning NULL-value\n");
        return NULL;
    } 
    return disk_header;
}

//DECOMMENTARE
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks)
{
    if(num_blocks < 0 || disk == NULL)
    {
        printf("Invalid param, returning\n");
        return;
    } 

    int fd, ret;

    DiskHeader* hdr = NULL;

    int bitmap_size = (num_blocks / BYTE_DIM) + 1; // calculates how big the bitmap should be

    size_t header_size = sizeof(DiskHeader) + bitmap_size;
    //size_t header_size = sizeof(DiskHeader) + bitmap_size + num_blocks*BLOCK_SIZE;

    // opens the file (creating it if necessary)
    if(access(filename, F_OK) == 0) //already exists
    {   //apertura file
        
        fd = open(filename, O_RDWR , 0666);
        
        if(fd == -1)
        {
            printf("Error opening file, fd is -1, A\n");
            return;
        } 
        //ensuring disk space
        
        /*ret = posix_fallocate(fd, 0, header_size);
        
        if(ret)  
        {
            close(fd);
            handle_error_en("Error f-allocating", ret); //fallocate non setta errno
        }*/

        hdr = DiskDriver_initialize_header(hdr, fd, header_size);

        disk->header = hdr;
        disk->bitmap_data = (char*) hdr + sizeof(DiskHeader); //puntatore alla mappa, skippo header
        disk->fd = fd;

         //prendiamo il primo blocco libero, l'indice, RICORDARSI DI DECOMMENTARE
        hdr->first_free_block = DiskDriver_getFreeBlock(disk, 0);
    }
    else //to create
    {
        fd = open(filename, O_CREAT | O_RDWR | O_TRUNC , 0666);
        if(fd == -1)
        {
            printf("Error opening file, fd is -1, B\n");
            return;
        } 

        ret = posix_fallocate(fd, 0, header_size);
        if(ret)  
        {
            close(fd);
            printf("Error f-allocating, returning\n");
            return;
        }

        DiskHeader* hdr = DiskDriver_initialize_header(hdr, fd, header_size);  
         // if the file was new compiles a disk header, and fills in the bitmap of appropriate size with all 0 (to denote the free space);
        hdr->num_blocks = num_blocks;
        hdr->bitmap_blocks = num_blocks;
        hdr->bitmap_entries = bitmap_size;
        hdr->free_blocks = num_blocks;
        hdr->first_free_block = 0;

        disk->header = hdr;
        disk->bitmap_data = (char*) hdr + sizeof(DiskHeader); //puntatore all'area mappata skipapto di DiskHeader
        disk->fd = fd;

        memset(disk->bitmap_data, 0, bitmap_size); //bit a zero
        //memset(disk->bitmap_data, '0', bitmap_size);
    }

    printf("Disk driver initialized, proceding."); printf("."); printf(".\n");
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
    if(block_num < 0 || block_num >= disk->header->num_blocks )
    {
        printf("Invalid param (block_num), returning -1");
        return -1;
    }
    if(disk == NULL)
    {
        printf("Invalid param (disk), returning -1");
        return -1;
    }
    if(dest == NULL)
    {
        printf("Invalid param (dest), returning -1");
        return -1;
    }

    BitMap bm;
    bm.num_bits = disk->header->bitmap_blocks;
    bm.entries = disk->bitmap_data;

    if(BitMap_get(&bm, block_num, 0) == block_num) //cerco un blocco libero (== 0) partendo proprio da block num
    {
        printf("Block is free, returning -1\n");
        return -1; //blocco libero, nulla da leggere
    }
    int fd = disk->fd;
    int pos = sizeof(DiskHeader) + disk->header->bitmap_entries + (block_num*BLOCK_SIZE);
    //mi sposto all'offset e leggo byte per byte un blocco

    if(lseek(fd, pos, SEEK_SET) == -1) 
    {
        printf("FSEEK_error: Invalid offset, %d, returning -1\n", pos);
    }

    int ret, bytes_reads = 0;
    
	while(bytes_reads < BLOCK_SIZE)
    { 
        //lettura byte x byte blocco dim fissa
		ret = read(fd, dest + bytes_reads, BLOCK_SIZE - bytes_reads);

		if (ret == -1)
        {
            if(errno == EINTR) continue;
            else
            {
                printf("Error while reading, returning -1\n");
                return -1;
            }
        }
		if (ret == 0) break;
		
		bytes_reads +=ret;
	}

    printf("Done\n");
    return 0;
}


// writes a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
//DECOMMENTARE
int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num)
{
    if(block_num < 0 || block_num >= disk->header->num_blocks )
    {
        printf("Invalid param (block_num), returning -1\n");
        return -1;
    }
    if(disk == NULL)
    {
        printf("Invalid param (disk), returning -1\n");
        return -1;
    }
    if(src == NULL) 
    {
        printf("Invalid param (src), returning -1\n");
        return -1;
    }

    BitMap bm;
    bm.num_bits = disk->header->bitmap_blocks;
    bm.entries = disk->bitmap_data;

    if(BitMap_get(&bm, block_num, 1) == block_num)
    {
        printf("Block already in use, returning -1\n"); return -1; //blocco già in utilizzo, nulla da leggere
    }

    int fd = disk->fd;
    int pos = sizeof(DiskHeader) + disk->header->bitmap_entries + (block_num*BLOCK_SIZE);
    //mi sposto all'offset e leggo byte per byte un blocco

    if(lseek(fd, pos, SEEK_SET) == -1) 
    {
        printf("FSEEK_error: Invalid offset, %d, returning -1\n", pos);
        return -1;
    }

    int ret, bytes_w = 0;

    while (bytes_w < BLOCK_SIZE) 
    {
        ret = write(fd, src + bytes_w, BLOCK_SIZE - bytes_w);
        if (ret == -1)
        {
            if(errno == EINTR) continue;
            else
            {
                printf("Error while reading, returning -1\n");
                return -1;
            }
        }
        bytes_w += ret;
    }
    if(ret != BLOCK_SIZE)
    {
        printf("Error while reading"); return -1;
    }
    //alterare stato blocco in bitmap

    ret = BitMap_set(&bm, block_num, 1);
    if(ret) 
    {
        printf("Impossibile modificare stato del blocco desiderato"); return -1;
    }
    disk->header->free_blocks -= 1;
    
    disk->header->first_free_block = DiskDriver_getFreeBlock(disk, block_num + 1);
    return 0; 
}

// frees a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_freeBlock(DiskDriver* disk, int block_num)
{
    if(block_num < 0 || block_num >= disk->header->num_blocks ) 
    {
        printf("Invalid param (block_num), returning -1\n");
        return -1;
    }
    if(disk == NULL)
    {
        printf("Invalid param (disk), returning -1\n");
        return -1;
    }
    

    BitMap bm;
    bm.num_bits = disk->header->bitmap_blocks;
    bm.entries = disk->bitmap_data;

    if(BitMap_get(&bm, block_num, 0) == block_num)
    {
        printf("Block already free, no action needed\n"); return 0; 
    }

    int ret;

    ret = BitMap_set(&bm, block_num, 0);
    if(ret) 
    {
        printf("Impossibile liberare il blocco desiderato"); return -1;
    }

    disk->header->free_blocks += 1;

    if(block_num < disk->header->first_free_block) disk->header->first_free_block = block_num;
   
    return 0; 
}

// returns the first free block in the disk from position (checking the bitmap)
int DiskDriver_getFreeBlock(DiskDriver* disk, int start)
{
    if(start < 0 || start > disk->header->num_blocks) 
    {
        printf("Invalid param (start), returning -1\n");
        return -1;
    }
    //printf("OOOK2\n");
    if(disk == NULL) 
    {
        printf("Invalid param (disk), returning -1\n");
        return -1;
    }
    
    BitMap bm;
    bm.num_bits = disk->header->bitmap_blocks;
    bm.entries = disk->bitmap_data;
    
    int ret = BitMap_get(&bm, start, 0);
    if(ret == -1)
    {
        printf("NO free block, returning -1\n");
        return -1;
    }
}

// writes the data (flushing the mmaps)
int DiskDriver_flush(DiskDriver* disk)
{
    if(disk == NULL)
    {
        printf("Invalid param (disk), returning -1\n");
        return -1;
    }

    printf("Flushing...\n");
   
    int size = sizeof(DiskHeader) + (disk->header->num_blocks / BYTE_DIM) + 1;
    //controllare correttezza size

    /*
       Without use of this call, there is no guarantee that changes are
       written back before munmap(2) is called.  To be more precise, the
       part of the file that corresponds to the memory area starting at
       addr and having length length is updated.
    */

    if(msync(disk->header, size, MS_SYNC) == -1)
    {
        printf("Cannot execute msync, returning -1\n");
        return -1;
    } 

    return 0;
}

void DiskDriver_print(DiskDriver* disk)
{
    if(disk == NULL)
    {
        printf("Invalid param (disk), returning\n");
        return -1;
    }

    DiskHeader* hdr = disk->header;
    int free_space = (hdr->free_blocks)*BLOCK_SIZE;
    int used_space = (hdr->num_blocks - hdr->free_blocks)*BLOCK_SIZE;

    printf("Number of blocks: %d\n", hdr->num_blocks);
    printf("Free blocks: %d\n", hdr->free_blocks);
    printf("First free block: %d\n", hdr->first_free_block);
    printf("Used space: %d\tFree space: %d\n", used_space, free_space);
}