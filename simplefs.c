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

    //int fdb_space = (BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock) - sizeof(int))/sizeof(int); //massimo spazio necessario per la struct, preso dal .h
    //int db_space = (BLOCK_SIZE - sizeof(BlockHeader) - sizeof(int))/sizeof(int);


    if(fdb->num_entries > 0)
    {
        FirstFileBlock temp;

        for(int i = 0; i < FDB_SPACE; i++)
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

            for(int i = 0; i < DB_SPACE; i++)
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
        DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
        return NULL;
    }

    FirstFileBlock* file = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    memset(file, 0, sizeof(FirstFileBlock)); //cleared

    file->header.next_block = -1; file->header.previous_block = -1; file->header.block_in_file = 0; //default values
    strcpy(file->fcb.name,filename); 
    file->fcb.block_in_disk = new_block;
    file->fcb.directory_block = fdb->fcb.block_in_disk;
    file->fcb.is_dir = 0;
    //file->fcb.size_in_blocks = 0;  file->fcb.size_in_bytes = 0;

    //ok now we put the block on the disk

    int ret = DiskDriver_writeBlock(disk, file, new_block);
    if(ret == -1)
    {
        printf("SimpleFS_createFile: error while writing block on the disk\n");
        DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
		return NULL;
    }
    //now assign the file to a  dir

    int block_found = 0;
	int block_entry = 0; //for tracking
	int no_space_fdb = 0;
	int block_number = fdb->fcb.block_in_disk;
	int block_in_file = 0;
	int fdb_or_db = 0; // 0 if fdb

    DirectoryBlock aux;

    if(fdb->num_entries < FDB_SPACE)
    {
        int* blocks = fdb->file_blocks;
        for(int j = 0; j < FDB_SPACE; j++)
        {
			if(blocks[j] == 0) //blocco libero, c'è spazio
            {  
				block_found = 1;
				block_entry = j;
				break;
			}
        }
    }
    else
    { //provo nel directory block
        no_space_fdb = 1;
        int next_block = fdb->header.next_block;

        while(next_block != -1 && !block_found) //finche ho un nuovo blocco libero e finche non ho trovato uno spazio
        { 
			ret = DiskDriver_readBlock(disk,&aux,next_block);
			if(ret == -1)
            {
				printf("SimpleFS_createFile: error while reading blocks (DB)\n");
				DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
				return NULL;
			}//leggo come prima
            int* blocks = fdb->file_blocks;		//cerco nello stesso modo di prima
			block_number = next_block;
			block_in_file++;
			for(int i = 0; i < DB_SPACE;i++)
            {
				if(blocks[i] == 0)
                {
					block_found = 1;
					block_entry = i;
					break;
				}
			}
            fdb_or_db = 1;
			next_block = aux.header.next_block;
        } //eow
    }
    if(!block_found) //allocate nu db with its header
    {
        DirectoryBlock new_db = {0};
		new_db.header.next_block = -1;
		new_db.header.block_in_file = block_in_file;
		new_db.header.previous_block = block_number;
		new_db.file_blocks[0] = new_block;

        int free_db = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
        if(free_db == -1)
        {
            printf("SimpleFS_createFile: error while asking for a free block_2\n");
            DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
            return NULL;
        }

        ret = DiskDriver_writeBlock(disk, &new_db, free_db);
        if(ret == -1)
        {
				printf("SimpleFS_createFile: error while writing a block\n");
				DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
				return NULL;
			}

        if(fdb_or_db == 0 ) //change pos in next block (fdb or db)
        {		
		    fdb->header.next_block = free_db 
		} 
        else
        {	
			aux.header.next_block = free_db;
		}
		aux = new_db;			
		block_number = free_db;
    }

    if(no_space_fdb == 0){ //il file è in fdb
			fdb->num_entries++;
			fdb->file_blocks[block_entry] = new_block;
			DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
			DiskDriver_writeBlock(disk, fdb, fdb->fcb.block_in_disk);
		}else{//il file è nei directory blocks
			fdb->num_entries++;
			DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
			DiskDriver_writeBlock(disk,fdb,fdb->fcb.block_in_disk);
			aux.file_blocks[block_entry] = new_block;
			DiskDriver_freeBlock(disk,block_number);
			DiskDriver_writeBlock(disk,&aux,block_number);
		}

	FileHandle* fh = malloc(sizeof(FileHandle));
	fh->sfs = d->sfs;
	fh->fcb = file;
	fh->directory = fdb;
	fh->pos_in_file = 0;

	return fh;
}

// reads in the (preallocated) blocks array, the name of all files in a directory 
int SimpleFS_readDir(char** names, DirectoryHandle* d, int* flag)
{
    if(names == NULL) handle_error_en("Invalid param (Names)", EINVAL);
    if(d == NULL) handle_error_en("Invalid param (DirHandle)", EINVAL);
    if(flag == NULL) handle_error_en("Invalid param (flags)", EINVAL);

    int count = 0;

    FirstDirectoryBlock* dcb = d->dcb;
    DiskDriver* disk = d->sfs->disk;

    if(dcb->num_entries <= 0)
    {
        printf("SimpleFS_readDir: empty directory\n");
        return -1;
    }


    FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    if(ffb == NULL)
    {
		printf("SimpleFS_readDir: malloc error on db\n");
		return -1;
	}

    int* blocks = dcb->file_blocks;
    for(int i = 0; i < FDB_SPACE; i++)
    {
        if(blocks[i] > 0 && DiskDriver_readBlock(disk, ffb, blocks[i]) != -1)
        {
            strncpy(names[count], ffb->fcb.name, 128);
            flag[count] = ffb->fcb.is_dir; //salvo nella corrispondente pos il flag
            count++;
        }
    }

    if(dcb->num_entries > FDB_SPACE)
    {
        int next = dcb->header.next_block;
		DirectoryBlock db;

		while (next != -1)
        {	
			int ret = DiskDriver_readBlock(disk, &db, next); // read new directory block
			if (ret == -1)
            {
				printf("SimpleFS_readDir: error while reading\n");
				return -1;
			}
			int* blocks = db.file_blocks;
            for(int i = 0; i < DB_SPACE; i++)
            {
                if(blocks[i] > 0 && DiskDriver_readBlock(disk, ffb, blocks[i]) != -1)
                {
                    strncpy(names[count], ffb->fcb.name, 128);
                    flag[count] = ffb->fcb.is_dir; //salvo nella corrispondente pos il flag
                    count++;
                }
            }
            next = db.header.next_block;
        }
    }
    return 0; //aight
}

// opens a file in the  directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename)
{
    if(filename == NULL) handle_error_en("Invalid param (filename)", EINVAL);
    if(d == NULL) handle_error_en("Invalid param (DirHandle)", EINVAL);

    DiskDriver* disk = d->sfs->disk;
    FirstDirectoryBlock* fdb = d->dcb;
    if(fdb->num_entries == 0)
    {
        printf("SimpleFS_openFile: empty directory\n");
		return NULL;
    }
    if(fdb->num_entries > 0)
    {
        FileHandle* fh = (FileHandle*) malloc(sizeof(FileHandle));
        fh = d->sfs;
        fh->directory = fdb;
        fh->pos_in_file = 0;

        int found = 0;

        FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
        for(int i = 0; i < FDB_SPACE; i++)
        {
            if(fdb->file_blocks[i] > 0 && DiskDriver_readBlock(disk, ffb, fdb->file_blocks[i]) != -1)
            {
               if(strncmp(ffb->fcb.name,filename,128) == 0 )
               {
                   
                   //DiskDriver_readBlock(disk, ffb, fdb->file_blocks[i]);
                   fh->fcb = ffb;
                   found = 1;
                   break;
               }
            }
        }
        //si va in directory blocks ora
        int next = fdb->header.next_block;
        DirectoryBlock db;
        
        while(next != -1 && !found)
        {
            if(DiskDriver_readBlock(disk, &db, next) == -1)
            {
                printf("SimpleFS_openFile: problem while reading blocks\n");
				return NULL;
            }

            for(int i = 0; i < DB_SPACE; i++)
            {
                if(db.file_blocks[i] > 0 && DiskDriver_readBlock(disk, ffb, db.file_blocks[i]) != -1)
                {
                    if(strncmp(ffb->fcb.name,filename,128) == 0 )
                    {
                        
                        //DiskDriver_readBlock(disk, ffb, fdb->file_blocks[i]);
                        fh->fcb = ffb;
                        found = 1;
                        break;
                    }                    
                }
            }
        next = db.header.next_block;
        }

        if(found)
        {
            return fh;
        }
        else
        {
            printf("SimpleFS_openFile: file does not exist\n");
            free(fh);
            return NULL;
        }
    }
    //return NULL;
}

// closes a file handle (destroyes it)
int SimpleFS_close(FileHandle* f)
{
    if(f == NULL) handle_error_en("Invalid param (filehandle)", EINVAL);

    free(f->fcb);
    free(f);

    return 0;
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle* f, void* data, int size)
{
    //invalid param check
    if(f == NULL) handle_error_en("Invalid param (filehandle)", EINVAL);
    if(data == NULL) handle_error_en("Invalid param (data)", EINVAL);
    if(size < 0) handle_error_en("Invalid param (size)", EINVAL);

    FirstFileBlock* ffb = f->fcb; //ffb extraction
    DiskDriver* disk = f->sfs->disk; // disk extraction

    int written_bytes = 0;
    int offset = f->pos_in_file;
    int btw = size;

    if(offset < FFB_SPACE && size <= FFB_SPACE - offset) //1 - best scenario
    {
        memcpy(ffb->data + offset, (char*) data, size);
        written_bytes = size;
        //updating
        DiskDriver_freeBlock(disk,ffb->fcb.block_in_disk);
        DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);

        return written_bytes;
    } //BEST SCENARIO ENDS
    else if(offset < FFB_SPACE && size > FFB_SPACE - offset) //2 - offset ancora in ffb, ma i byte da scrivere non entrano tutti
    {
        memcpy(ffb->data+offset, (char*)data, FFB_SPACE - offset);
		written_bytes += FFB_SPACE - offset;
		btw = size - written_bytes;
		DiskDriver_freeBlock(disk,ffb->fcb.block_in_disk);
		DiskDriver_writeBlock(disk, ffb, ffb->fcb.block_in_disk);
		offset = 0;
    }
    else offset -= FFB_SPACE;//3 - pos fuori dall'ffb, bisogna proprio cambiare blocco
    
    int next_block = ffb->header.next_block;

    int block_in_disk = ffb->fcb.block_in_disk;
	int block_in_file = ffb->header.block_in_file;
	FileBlock temp;

    int onlyblock = 0;
    if(!next_block) onlyblock = 1;

    while(written_bytes < size)
    {
        if(!next_block) //non ci sono blocchi, lo creiamo
        {
            FileBlock new_fb = {0};
		    new_fb.header.block_in_file = block_in_file + 1;
		    new_fb.header.next_block = -1;
		    new_fb.header.previous_block = block_in_disk;

    		next_block = DiskDriver_getFreeBlock(disk,block_in_disk);

            if(onlyblock == 1)
            {
                ffb->header.next_block = next_block;
			    DiskDriver_freeBlock(disk, block_in_disk);
			    DiskDriver_writeBlock(disk,ffb, block_in_disk); //ffb->fcb.block_in_disk;
			    onlyblock = 0;
            }
            else
            {
                temp.header.next_block = next_block;
			    DiskDriver_freeBlock(disk, block_in_disk);
			    DiskDriver_writeBlock(disk,&temp, block_in_disk);
            }
            DiskDriver_writeBlock(disk, &new_fb, next_block);
            temp = new_fb;
        }
        else if(DiskDriver_readBlock(disk,&temp,next_block) == -1) return -1;

        if(offset < FB_SPACE && size <= FB_SPACE - offset)
        {
            memcpy(temp.data+offset, (char*)data, FB_SPACE - offset);
			written_bytes += btw;

	        DiskDriver_freeBlock(disk, block_in_disk);
			DiskDriver_writeBlock(disk, ffb, block_in_disk);
			//DiskDriver_freeBlock(disk, block_in_disk);
            DiskDriver_freeBlock(disk, next_block);
			DiskDriver_writeBlock(disk, &temp, next_block);

			return written_bytes;
        }
        else if(offset < FB_SPACE && btw > FB_SPACE - offset)
        {
            memcpy(temp.data+offset,(char*)data + written_bytes, FB_SPACE - offset);
			written_bytes += FB_SPACE - offset;
			btw = size - written_bytes;
			DiskDriver_freeBlock(disk, block_in_disk);
			DiskDriver_writeBlock(disk, &temp, next_block);
			offset = 0;
        }
        else offset -= FB_SPACE;

		block_in_disk = next_block;	// update index of current_block
		next_block = temp.header.next_block;
		block_in_file = temp.header.block_in_file; // update index of next_block
    }//EOW
    return written_bytes;
}


int SimpleFS_read(FileHandle* f, void* data, int size)
{
    //invalid param check
    if(f == NULL) handle_error_en("Invalid param (filehandle)", EINVAL);
    if(data == NULL) handle_error_en("Invalid param (data)", EINVAL);
    if(size < 0) handle_error_en("Invalid param (size)", EINVAL);

    FirstFileBlock* ffb = f->fcb;
    DiskDriver* disk = f->sfs->disk;
    int offset = f->pos_in_file;
    int bytes_read = 0;
    int readable = size;

    memset(data, '\0', size); 

    if(offset < FFB_SPACE && size <= FFB_SPACE - offset)
    {
        memcpy(data, ffb->data + offset, size);
        f->pos_in_file += size;
        return size;
    }
    else if(offset < FFB_SPACE && size > FFB_SPACE - offset)
    {
        memcpy(data, ffb->data + offset, FFB_SPACE - offset);
        bytes_read += FFB_SPACE - offset;
        readable = size - bytes_read;
        offset = 0;
    }
    else offset -= FFB_SPACE;

    int next_block = ffb->header.next_block;
    FileBlock temp;
    while(readable < size && next_block != -1)
    {
        if(DiskDriver_readBlock(disk, &temp, next_block) == -1) return -1;

        if(offset < FB_SPACE && size <= FB_SPACE - offset)
        {
            memcpy(data + bytes_read, temp.data + offset, readable);
            bytes_read += readable;
			readable = size - bytes_read;
			f->pos_in_file = bytes_read;
			return bytes_read;
        }
        else if(offset < FB_SPACE && size > FB_SPACE - offset)
        {
            memcpy(data + bytes_read, temp.data + offset, readable);
            bytes_read += readable;
			readable = size - bytes_read;
            offset = 0;
        }
        else offset -= FB_SPACE;

        next_block = temp.header.next_block;
    }//EOW
    return bytes_read;
}

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success
// -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos)
{
    if(f == NULL) handle_error_en("Invalid param (FileHandle)", EINVAL);
    if(pos < 0) handle_error_en("Invalid param (pos)", EINVAL);

    int size = 0;
    FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));

    if(DiskDriver_readBlock(f->sfs->disk, ffb, f->fcb->fcb.block_in_disk) == -1) handle_error_en("SimpleFS_seek: Error while reading block on disk", EIO);

    size += sizeof(ffb->data);
    
    int next_block = ffb->header.next_block;
    FileBlock temp;
    while(next_block != -1)
    {
        if(DiskDriver_readBlock(f->sfs->disk, &temp, next_block) == -1) handle_error_en("SimpleFS_seek: Error while reading block on disk", EIO);
        size += sizeof(temp.data);

        next_block = temp.header.next_block;
    }

    if(pos > size)
    {
        free(ffb);
        printf("SimpleFS_seek: file's too short, returning -1");
        return -1;
    }
    f->pos_in_file = pos;
    return pos;
}

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname)
{
    if(d == NULL) handle_error_en("Invalid param (Directory)", EINVAL);
    if(dirname == NULL) handle_error_en("Invalid param (Directory name)", EINVAL);

    int ret = 0;
    DiskDriver* disk = d->sfs->disk;

    if(strcmp(dirname, "..") == 0)//torno a directory superiore, prior check per vedere se sono già la root
    {
        if(d->directory == NULL)
        {
            printf("SimpleFS_changeDir: we already in root directory, cannot proceed\n");
            return -1;
        }
        
        d->dcb = d->directory;
        d->pos_in_block = 0;
        d->pos_in_dir = 0;
        int p_block = d->dcb->fcb.directory_block;

        if(p_block == -1) //case: we in root now
        {
            d->directory = NULL;
            return 0;
        }

        FirstDirectoryBlock* p = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
        ret = DiskDriver_readBlock(disk, p, p_block);
        if(ret == -1)
        {
            printf("SimpleFS_changeDir: cannot read parent directory of the directory you want to hop in\n");
            d->directory = NULL;
            return -1;
        }
        d->directory = p;
        return 0;
    }
    else if(d->dcb->num_entries <= 0)
    {
        printf("SimpleFS_changeDir: empty dir\n");
        return -1;
    }
    //normal cd now
    FirstDirectoryBlock* fdb = d->dcb;
    FirstDirectoryBlock* temp = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));

    for(int i = 0; i < FDB_SPACE; i++)
    {
        if(fdb->file_blocks[i] > 0 && (DiskDriver_readBlock(disk,temp,fdb->file_blocks[i])) != -1)
        {
            if(strcmp(temp->fcb.name,dirname) == 0){
					//DiskDriver_readBlock(disk,temp,fdb->file_blocks[i]);
					d->pos_in_block = 0; 
					d->directory = fdb;
					d->dcb = temp;
					return 0;
				}
        }
    }

    int next_block = fdb->header.next_block;

    DirectoryBlock db;
    while(next_block != -1)
    {
        ret = DiskDriver_readBlock(disk, &db, next_block);
		if(ret == -1)
        {
			printf("SimpleFS_changeDir: cannot read the block\n");
            free(temp);
			return -1;
		}

        for(int i = 0; i < DB_SPACE; i++)
        {
            if(db.file_blocks[i] > 0 && (DiskDriver_readBlock(disk, temp, db.file_blocks[i])) != -1)
            {
                if(strcmp(temp->fcb.name,dirname) == 0){
					//DiskDriver_readBlock(disk, temp, db.file_blocks[i]);
					d->pos_in_block = 0; 
					d->directory = fdb;
					d->dcb = temp;
					return 0;
				}
            }
        }
        next_block = db.header.next_block;
    }
    printf("SimpleFS_changeDir: cannot change directory\n");
    return -1;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname)
{
    if(d == NULL) handle_error_en("Invalid param (Directory)", EINVAL);
    if(dirname == NULL) handle_error_en("Invalid param (Directory name)", EINVAL);

    DiskDriver* disk = d->sfs->disk;
    FirstDirectoryBlock* dcb = d->dcb;
    FirstDirectoryBlock* temp = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock)); //o first file block?
    if(temp == NULL){
		printf("SimpleFS_mkDir: malloc (temp) error while creating FirstDirecotryBlock\n");
		return -1;
	}
    //se la cartella non è vuota, devo controllare se voglio creare una cartella con un nome già utilizzato
    if(d->dcb->num_entries > 0)
    {
        for(int i = 0; i < FDB_SPACE; i++)
        {
            if(dcb->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, temp, dcb->file_blocks[i])) != -1)
            {
                if(strcmp(temp->fcb.name, dirname) == 0)
                {
                    free(temp);
                    printf("SimpleFS_mkDir: directory_name already exists, try another name\n");
                    return -1;
                }
            }
        }

        DirectoryBlock db;
        int next_block = dcb->header.next_block;

        while(next_block != -1)
        {
            int ret = DiskDriver_readBlock(disk, &db, next_block);
            if(ret == -1)
            {
                printf("SimpleFS_changeDir: cannot read the block\n");
                free(temp);
			    return -1;
            }
            for(int i = 0; i < DB_SPACE; i++)
            {
                if(db.file_blocks[i] > 0 && (DiskDriver_readBlock(disk, temp, db.file_blocks[i])) != -1)
                {
                    if(strcmp(temp->fcb.name, dirname) == 0)
                    {
                    free(temp);
                    printf("SimpleFS_mkDir: directory_name already exists, try another name\n");
                    return -1;
                    }
                }
            }
            next_block = db.header.next_block;
        }
    } //already exists check passed

    int nb = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
    if(nb == -1)
    {
        printf("SimpleFS_mkDir: error while asking disk for a free block\n");
        return -1;
    }

    FirstDirectoryBlock* new_dir = (FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
	if(new_dir == NULL){
		printf("SimpleFS_mkDir: (new_dir) malloc error while creating FirstDirecotryBlock\n");
		return -1;
	}

    new_dir->header.next_block = -1; new_dir->header.previous_block = -1; new_dir->header.block_in_file = 0;

    new_dir->fcb.directory_block = dcb->fcb.block_in_disk; new_dir->fcb.block_in_disk = nb; new_dir->fcb.is_dir = 1;
    new_dir->fcb.size_in_blocks = 0; new_dir->fcb.size_in_bytes = 0; 
    new_dir->num_entries = 0;
	strcpy(new_dir->fcb.name, dirname);

    int block_number = dcb->fcb.block_in_disk;
    //scrivere su disco la directory

    DirectoryBlock db;

    int ret = DiskDriver_writeBlock(disk, new_dir, nb);
    if(ret == -1)
    {
        printf("SimpleFS_mkDir: cannot write new block on disk\n");
        return -1;
    }
    int block_in_file = 0;
    int fdb_or_db = 0; //variabile utilizzata x capire se, nel caso non ci fosse un blocco libero nell'intera cartella, se doversi attaccari all'fdb o a un semplice db

    if(dcb->num_entries < FDB_SPACE)
    {
        for(int i = 0; i < FDB_SPACE; i++)
        {
            if(dcb->file_blocks[i] == 0)
            {
                dcb->num_entries++;
		        dcb->file_blocks[i] = nb;
			    //update free then write
		        DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		        DiskDriver_writeBlock(disk, dcb, dcb->fcb.block_in_disk);
                return 0;
            }
        }
    }
    else
    {
        int next = dcb->header.next_block;
        
        while(next != -1)
        {
            ret = DiskDriver_readBlock(disk, &db, next);
            if(ret == -1)
            {
                printf("SimpleFS_mkDir: cannot read next block from disk\n");
                DiskDriver_freeBlock(disk,dcb->fcb.block_in_disk);
                return -1;
            }
            block_in_file++;
            block_number = next;

            for(int i = 0; i < DB_SPACE; i++)
            {
                if(db.file_blocks[i] == 0)
                {
                    dcb->num_entries++;

                    DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		            DiskDriver_writeBlock(disk, dcb, dcb->fcb.block_in_disk);
		            db.file_blocks[i] = nb;
                    DiskDriver_freeBlock(disk, block_number);
		            DiskDriver_writeBlock(disk, &db, block_number);
                    return 0;
                }
            }
            fdb_or_db = 1;
			next = db.header.next_block;
        }
    } 

    DirectoryBlock new_db = {0};
	new_db.header.previous_block = block_number;
	new_db.header.next_block = -1;
	new_db.header.block_in_file = block_in_file;
	new_db.file_blocks[0] = nb;

    int nd_block = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
	if(nd_block == -1)
    {
		printf("Cannot get a new free block: SimpleFS_mkDir\n");
		DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		return -1;
	}

	ret = DiskDriver_writeBlock(disk, &new_db, nd_block); //scrivo il blocco su disco
	if(ret == -1)
    {
		printf("Cannot write new_dir_block on the disk: SimpleFS_mkDir\n" );
		DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		return -1;
	}

    if(fdb_or_db == 0) dcb->header.next_block = nd_block;
	else db.header.next_block = nd_block;
			
	//db = new_db;
	//block_number = nd_block; codice inutile
    
	return 0;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, char* filename)
{
    if(d == NULL) handle_error_en("Invalid param (Directory)", EINVAL);
    if(filename == NULL) handle_error_en("Invalid param (Directory name)", EINVAL);

    DiskDriver* disk = d->sfs->disk;
    FirstDirectoryBlock* fdb = d->dcb;

    if(fdb->num_entries < 1)
    {
		printf("SimpleFS_remove: empty directory.\n");
		return -1;
	}

    FirstFileBlock ffb;
    int id = -1;
    int entry = -1;
    for(int i = 0; i < FDB_SPACE; i++)
    {
        if(fdb->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, &ffb, fdb->file_blocks[i])) != -1)
        {
            if(strcmp(filename, ffb.fcb.name) == 0)
            {   
                entry = i;
                id = fdb->file_blocks[i];
                break;
            }
        }
    }
    int firstfound = 1; //significa che il file è stato trovato nel fdb

    DirectoryBlock* db = (FirstDirectoryBlock*) malloc(sizeof(DirectoryBlock));

    int next = fdb->header.next_block;
	int block_in_disk = fdb->header.block_in_file;

    while(id == -1 && next) //id = -1 significa che non sta nell'fdb
    {
        firstfound = 0;
        int ret = DiskDriver_readBlock(disk, &db, next);
        if(ret == -1)
        {
            printf("SimpleFS_remove: cannot read block\n");
            free(db);
		    return -1;
        }

        for(int i = 0; i < DB_SPACE; i++)
        {
            if(db->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, &ffb, db->file_blocks[i])) != -1)
            {
                if(strcmp(filename, ffb.fcb.name) == 0)
                {
                    entry = i;
                    id = db->file_blocks[i];
                    break;
                }
            }
        }
        block_in_disk = next;
        next = db->header.next_block;
    }

    if(id == -1)
    {
        printf("SimpleFS_remove: file does not exist!\n");
        free(db);
		return -1;
    }

    FirstFileBlock kill;
    if(DiskDriver_readBlock(disk, &kill, id) == -1)
    {
        printf("SimpleFS_remove: cannot read file to kill\n");
        free(db);
        return -1;
    } 

    if(!kill.fcb.is_dir) //eliminazioe file
    {
        FileBlock aux;
        next = kill.header.next_block; //prima era ridefinito
        int block = id;

        while(next != -1)
        {
            if(DiskDriver_readBlock(disk, &aux, next) == -1) return -1;
			block = next;
			next = aux.header.next_block;
			DiskDriver_freeBlock(disk, block);
        }
        if(DiskDriver_freeBlock(disk, id) == -1)
        {
            printf("SimpleFS_remove: cannot free the block\n");
            free(db);
		    return -1;
        }

        d->dcb = fdb;
    }

    else //eliminazione cartella e file all'interno
    {
        FirstDirectoryBlock fdb_kill;
        if(DiskDriver_readBlock(disk, &fdb_kill, id) == -1)
        {
            printf("SimpleFS_remove: cannot read dir to kill\n");
            free(db);
            return -1;
        } 

        if(fdb_kill.num_entries < 1) //cartella vuota
        {
            if(DiskDriver_freeBlock(disk, id) == -1)
                {
                    printf("SimpleFS_remove: cannot free the block\n");
                    free(db);
		            return -1;
                }

            d->dcb = fdb;
        }
        else //cartella !vuota
        {
            //entro nella cartella x eliminarne i file
            if(SimpleFS_changeDir(d, fdb_kill.fcb.name) == -1)
            {
                printf("SimpleFS_remove: cannot jump into the dir to kill its files\n");
                free(db);
		        return -1;
            }

            for(int i = 0; i < FDB_SPACE; i++)
            {   //chiamata ricorsiva
                if(fdb_kill.file_blocks[i] > 0 && DiskDriver_readBlock(disk,&ffb, fdb_kill.file_blocks[i]) != -1) SimpleFS_remove(d,ffb.fcb.name);
            }

            next = fdb_kill.header.next_block;
            int block = id;
            DirectoryBlock db_aux;
            while(next)
            {
                if(DiskDriver_readBlock(disk, &db_aux, next) == -1)
                {
                    printf("SimpleFS_remove: cannot read dirblock\n");
                    free(db);
		            return -1;
                }
                for(int i = 0; i < DB_SPACE; i++)
                {
					FirstFileBlock ffb_rec;

					if(DiskDriver_readBlock(disk, &ffb_rec, db_aux.file_blocks[i]) == -1)
                    {
                        printf("SimpleFS_remove: cannot read ffb_rec\n");
                        free(db);
                        return -1;
                    } 
					SimpleFS_remove(d, ffb_rec.fcb.name);
				}
				block_in_disk = next;
				next = db_aux.header.next_block;
				DiskDriver_freeBlock(disk,block_in_disk);
            }
            DiskDriver_freeBlock(disk, id);
			d->dcb = fdb;
        }

    }
    if(!firstfound)
    {
        	db->file_blocks[entry] = -1;
			fdb->num_entries -= 1;
      
            DiskDriver_freeBlock(d->sfs->disk, block_in_disk);
			DiskDriver_writeBlock(d->sfs->disk, db, block_in_disk);
            DiskDriver_freeBlock(d->sfs->disk, fdb->fcb.block_in_disk);
			DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->fcb.block_in_disk);
			return 0;
    }
    else
    {
        fdb->file_blocks[id] = -1;
		fdb->num_entries-=1;
     
        DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
		DiskDriver_writeBlock(disk, fdb, fdb->fcb.block_in_disk);
		return 0;
    }
    
}
  