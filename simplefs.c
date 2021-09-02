#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h> 

#include "simplefs.h"

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk)
{
    //ez controlli args
    if(disk == NULL) handle_error_en("Invalid param (disk)", EINVAL);
    if(fs == NULL) handle_error_en("Invalid param (Filesys)", EINVAL);

    fs->disk = disk;

    FirstDirectoryBlock* fdb = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));

    int ret = DiskDriver_readBlock(disk, fdb, 0);
    if(ret == -1){ 
        free(fdb);
        return NULL;
    }

    DirectoryHandle* dh = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));

    dh->sfs = fs; 
    dh->dcb = fdb;
    dh->directory = NULL;
    dh->pos_in_block = 0; 
    dh->pos_in_dir = 0;

    dh->current_block = (BlockHeader *) malloc(sizeof(BlockHeader));
	memcpy(dh->current_block, &(fdb->header), sizeof(BlockHeader));

    return dh;
}

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
void SimpleFS_format(SimpleFS* fs)
{
    if(fs == NULL) handle_error_en("Invalid param (Filesys)", EINVAL);

    FirstDirectoryBlock root = {0}; //TOP LEVEL
    root.header.block_in_file = 0;
    root.header.next_block = -1;
    root.header.previous_block = -1;

    for(int i = 0; i < root.num_entries; i++)
    {
        root.file_blocks[i] = -1;
    }

    root.fcb.directory_block = -1; //no parents
    root.fcb.block_in_disk = 0;
    root.fcb.is_dir = 1;
    strcpy(root.fcb.name, "/");
    //root dir ok, now clean disk

    fs->disk->header->free_blocks = fs->disk->header->num_blocks; //all block are free
    fs->disk->header->first_free_block = 0;

    int bm_size = fs->disk->header->bitmap_entries; 	
    memset(fs->disk->bitmap_data, '\0', bm_size); //bm cleared

    int ret = DiskDriver_writeBlock(fs->disk, &root, 0);
    if(ret == -1)
    {
        //nella write block o l'errore che capita setta già errno,
        //oppure c'è un problema con la write, che torna -1 ---> setto errno a -1
        handle_error_en("Error while writing first block", -1); 
    }
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename)
{
    if(d == NULL) handle_error_en("Invalid param (Directory)", EINVAL);
    if(filename == NULL) handle_error_en("Invalid param (filename)", EINVAL);

    SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;                   
	FirstDirectoryBlock* fdb = d->dcb;

    if(fs == NULL || disk == NULL || fdb == NULL)
    { 
	    printf("Bad parameters in SimpleFS_createFile\n");
	    return NULL;
	}
    //check per controllare se esiste già

    int fdb_space = (BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock) - sizeof(int))/sizeof(int); //massimo spazio necessario per la struct, preso dal .h
    int db_space = (BLOCK_SIZE - sizeof(BlockHeader) - sizeof(int))/sizeof(int);


    if(fdb->num_entries > 0)
    {
        FirstFileBlock temp;

        for(int i = 0; i < fdb_space; i++)
        {
            if((fdb->file_blocks[i] > 0 && DiskDriver_readBlock(disk, &temp, fdb->file_blocks[i])) != -1)
            {
                if(strcmp(fdb->fcb.name,filename) == 0)
                {
					printf("SimpleFS_createFile: file already exists\n");
					return NULL;
				}
            }
        } //next block

        int next_block = fdb->header.next_block;
	    DirectoryBlock db;

        while(next_block != -1)
        {
            //leggo il blocco (error check)
            //poi faccio stesso controllo di prima su fdb, ma su db
            int ret;
            ret = DiskDriver_readBlock(disk, &db, next_block);
            if(ret == -1)
            {
                printf("SimpleFS_createFile: error while reading DirectoryBlock\n");
                return NULL;
            }

            for(int i = 0; i < db_space; i++)
            {
                if((db.file_blocks[i] > 0 && DiskDriver_readBlock(disk, &temp, db.file_blocks[i])) != -1)
                {
                    if(strcmp(fdb->fcb.name,filename) == 0)
                    {
					    printf("SimpleFS_createFile: file already exists\n");
					    return NULL;
				    }
                }
            }
            next_block = db.header.next_block;
        }
    }//file does not previously exists
    int new_block = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
    if(new_block == -1)
    {
        printf("SimpleFS_createFile: error while asking for a free block\n");
    }

    FirstFileBlock* file = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    memset(file, 0, sizeof(FirstFileBlock)); //cleared

    file->header.next_block = -1; file->header.previous_block = -1; file->header.block_in_file = 0; //default values
    strcpy(file->fcb.name,filename); 
    file->fcb.block_in_disk = new_block;
    file->fcb.directory_block = fdb->fcb.block_in_disk;
    file->fcb.is_dir = 0;

    //ok now we put the block on the disk

    int ret = DiskDriver_writeBlock(disk, file, new_block);
    if(ret == -1)
    {
        printf("SimpleFS_createFile: error while writing block on the disk\n");
		return NULL;
    }
    //now assign the file toa  dir

}

//-----------------------------------------------------------------------------------------------------

