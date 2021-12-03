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
    if(disk == NULL) 
    {
        printf("Invalid param (disk)");
        return NULL;
    }
    if(fs == NULL) 
    {
        printf("Invalid param (fs)");
        return NULL;
    }

    fs->disk = disk;

    if(disk->header->first_free_block == 0 && disk->header->free_blocks == disk->header->num_blocks) SimpleFS_format(fs); //if sfs does not previously exist --> format
     

    FirstDirectoryBlock* fdb = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
    
    int ret = DiskDriver_readBlock(fs->disk, fdb, 0);
    if(ret == -1)
    { 
        free(fdb);
        return NULL;
    }
    
    DirectoryHandle* dh = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));

    dh->sfs = fs; 
    dh->dcb = fdb;
    dh->directory = NULL; //null if toplevel
    dh->pos_in_block = 0; 
    dh->pos_in_dir = 0;
    dh->current_block = &(fdb->header);//(BlockHeader *) malloc(sizeof(BlockHeader));
	//memcpy(dh->current_block, &(fdb->header), sizeof(BlockHeader)); //dest, source
    printf("Filesystem initialized successfully!\n\n");
    
    return dh;
}

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
void SimpleFS_format(SimpleFS* fs)
{
    if(fs == NULL) 
    {
        printf("Invalid param (fs)");
        return;
    }
    //initial structures
    FirstDirectoryBlock root = {0}; //TOP LEVEL DIR
    root.header.block_in_file = 0;
    root.header.next_block = -1;
    root.header.previous_block = -1;

    for(int i = 0; i < root.num_entries; i++) root.file_blocks[i] = 0;
    

    root.fcb.directory_block = -1; //no parents
    root.fcb.block_in_disk = 0;
    root.fcb.is_dir = 1;
    root.fcb.size_in_bytes = sizeof(FirstDirectoryBlock);
    root.fcb.size_in_blocks = root.fcb.size_in_bytes % BYTE_DIM == 0 ? (root.fcb.size_in_bytes / BLOCK_SIZE) : ((root.fcb.size_in_bytes / BLOCK_SIZE) + 1);
    root.fcb.written_bytes = 0;
    strcpy(root.fcb.name, "/");
    //root dir with parameters ok

    fs->disk->header->free_blocks = fs->disk->header->num_blocks; //all block are free
    fs->disk->header->first_free_block = 0;

    //bitmap op
    int bm_size = fs->disk->header->bitmap_entries; 	
    memset(fs->disk->bitmap_data, '\0', bm_size); //bm cleared

    int ret = DiskDriver_writeBlock(fs->disk, &root, 0);
    if(ret == -1)
    {
        printf("Error while writing first block\n"); 
        return;
    }
    DiskDriver_flush(fs->disk);
    printf("[FORMAT SUCCESS]\n");
}

int SimpleFS_readDir(char** names, DirectoryHandle* d, int* flag) {
    
    if(names == NULL)
    {
        printf("Invalid param (names)");
        return -1;
    }
    if(d == NULL) 
    {
        printf("Invalid param (dir_handle)");
        return -1;
    }
    if(flag == NULL) 
    {
        printf("Invalid param (flags)");
        return -1;
    }

    FirstDirectoryBlock* fdb = d->dcb;
    int entries = fdb->num_entries;
    int count = 0, ret;

    if(entries == 0)
    {
        printf("Empty directory\n");
        return 0;
    }
    if(entries < 0)
    {
        printf("SimpleFS_readDir: negative entries num!!!\n");
        return -1;
    }

    FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    memset(ffb, 0, sizeof(FirstFileBlock));

    for (int i = 0; i < FDB_SPACE; i++) 
    {
        if(entries == 0) 
        {
            free(ffb);  
            return 0;
        }
        int block_num = fdb->file_blocks[i];
        //printf("[shell] block num = %d\n", block_num);
        if(block_num > 0 && block_num < d->sfs->disk->header->num_blocks)
        {
            ret = DiskDriver_readBlock(d->sfs->disk, ffb, block_num);
            if (ret == -1)
            {
                printf("Readdir error, cannot keep going\n");
                free(ffb);  
                return -1;
            }
            //printf("%d\n", block_num);
            names[count] = strndup(ffb->fcb.name, 128);
            flag[count] = ffb->fcb.is_dir;
            count++;
            entries--;
        }      
        
    }
    
    
    if(entries == 0) return 0;

    DirectoryBlock db;
    int next = fdb->header.next_block;
    while(next != -1)
    {
        ret = DiskDriver_readBlock(d->sfs->disk, &db, next);
        if (ret == -1) {
            printf("Readdir error, cannot keep going\n");
            free(ffb);
            return -1;
        }
        for (int i = 0; i < DB_SPACE; i++) 
        {
            if(entries == 0)
            {
                free(ffb);  
                return 0;
            }   
            //FirstFileBlock ffb;
            int block_num = db.file_blocks[i];
            if(block_num > 0 && block_num < d->sfs->disk->header->num_blocks) //lettura blocco valido
            {
                ret = DiskDriver_readBlock(d->sfs->disk, ffb, block_num);
                if (ret == -1)
                {
                    printf("Readdir error, cannot keep going\n");
                    free(ffb);
                    return -1;
                }
                //printf("%d\n", block_num);
                names[count] = strndup(ffb->fcb.name, 128);
                flag[count] = ffb->fcb.is_dir;
                count++;
                entries--;
            }
        }
        if(entries == 0)
        {
            free(ffb);  
            return 0;
        } 
        next = db.header.next_block;
    }
    free(ffb);
    return 0;
}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename)
{
    if(d == NULL) 
    {
        printf("Invalid param (dir_handle)");
        return NULL;
    }
    
    if(filename == NULL) 
    {
        printf("Invalid param (filename)");
        return NULL;
    }
    
    SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;                   
	FirstDirectoryBlock* fdb = d->dcb;
    //printf("[test] next block: %d\n\n", fdb->header.next_block);

    if(fs == NULL || disk == NULL || fdb == NULL)
    { 
	    printf("Bad parameters in SimpleFS_createFile\n");
	    return NULL;
	}
    
    if(fdb->num_entries > 0)
    {
        FirstFileBlock temp;
        
        for(int i = 0; i < FDB_SPACE; i++)
        {
            //printf("%s\n", d->dcb->fcb.name);
            if((fdb->file_blocks[i] > 0 && DiskDriver_readBlock(disk, &temp, fdb->file_blocks[i]) != -1))
            {
                if(strcmp(temp.fcb.name,filename) == 0)
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
                if((db.file_blocks[i] > 0 && DiskDriver_readBlock(disk, &temp, db.file_blocks[i]) != -1))
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
    } //file does not previously exists
      //printf("\nExistence check:\t[PASSED]\n");
    //-----------------------------------------------------------------------------
    
    if(fs->disk->header->free_blocks < 1)
    {
        printf("Not enough space!\n");
        return NULL;
    }
    
    int new_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
    if(new_block == -1)
    {
        printf("SimpleFS_createFile: error while asking for a free block\n");
        return NULL;
    }
    //firstfileblock
    //setup file
    
    FirstFileBlock* file = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    if(file == NULL)
    {
		printf("SimpleFS_createFile: (new_dir) malloc error while creating FirstDirecotryBlock\n");
        free(file);
		return NULL;
	}
    memset(file, 0, sizeof(FirstFileBlock)); //cleared, così valgrind non piange

    file->header.next_block = -1; 
    file->header.previous_block = -1; 
    file->header.block_in_file = 0; //default values
    
    strcpy(file->fcb.name, filename); 

    file->fcb.directory_block = fdb->fcb.block_in_disk;
    file->fcb.block_in_disk = new_block;
    file->fcb.is_dir = 0;
    file->fcb.size_in_blocks = 1;
    file->fcb.size_in_bytes = BLOCK_SIZE;
    file->fcb.written_bytes = 0;
    //setup filehandle
    
    FileHandle* fh = (FileHandle*)malloc(sizeof(FileHandle));
	fh->sfs = d->sfs;
	fh->fcb = file;
	fh->directory = d->dcb;
	fh->pos_in_file = 0;
    fh->current_block = &(fh->fcb->header);

    //-----------------------------set up ended---------------------------------------

    //ok now we put the block on the disk

    int ret = DiskDriver_writeBlock(disk, file, new_block);
    if(ret == -1)
    {
        printf("SimpleFS_createFile: error while writing block on the disk\n");
        free(file); free(fh);
		return NULL;
    }
    //blocco scritto, tocca assegnarlo alla directory, capendo a quale blocco deve attaccarsi

	int block_in_file = 0;
	int block_number = fdb->fcb.block_in_disk;

    DirectoryBlock aux;

    if(fdb->num_entries < FDB_SPACE) //c'è spazio nell'fdb
    {
        for(int j = 0; j < FDB_SPACE; j++)
        {
			if(fdb->file_blocks[j] == 0) //blocco libero, c'è spazio
            {  
                fdb->file_blocks[j] = new_block; //setto il blocco
                fdb->num_entries++; 
                fdb->fcb.size_in_blocks++; 
                fdb->fcb.size_in_bytes += BLOCK_SIZE;

                if(DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk) == -1)
                {
                    printf("SimpleFS_createFile: error while freeing a block\n");
                    free(file); free(fh);
                    return NULL;
                }
		        if(DiskDriver_writeBlock(disk, fdb, fdb->fcb.block_in_disk) == -1)
                {
                    printf("SimpleFS_createFile: error while freeing a block\n");
                    free(file); free(fh);
                    return NULL;
                }
                //printf("File %s created\n", fh->fcb->fcb.name);
                //printf("File %s created\t[FDB]\tblock in disk: %d\t\t", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);
                //printf("Directory where contained: %s\n", fh->directory->fcb.name);

                return fh;
			}
        }        
    }
    else
    { //provo nel directory block

        int next_block = fdb->header.next_block;
        while(next_block != -1 ) //finche c'è un next db e finche non ho trovato uno spazio
        { 
			ret = DiskDriver_readBlock(disk,&aux,next_block); //leggo 
			if(ret == -1)
            {
				printf("SimpleFS_createFile: error while reading blocks (DB)\n");
                free(file); free(fh);
				return NULL;
			}//leggo come prima

			block_number = next_block; //rappresenterà il block in disk dell'ultimo blocco visitato, e sarà utile per settare il previous del nuvo db che andremo a creare
			block_in_file++; //nell'hdr del nuovo db

			for(int i = 0; i < DB_SPACE;i++)
            {
				if(aux.file_blocks[i] == 0)
                {
                    aux.file_blocks[i] = new_block;
                    
                    fdb->num_entries++; fdb->fcb.size_in_blocks++; fdb->fcb.size_in_bytes += BLOCK_SIZE;
                    //updating directory block
                    if(DiskDriver_freeBlock(disk, next_block) == -1) 
                    {
                        printf("SimpleFS_createFile: error while freeing a block\n");
                        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
                        free(file); free(fh);
                        return NULL;
                    }
                    //scrivo su disco in pos free_db
                    if(DiskDriver_writeBlock(disk, &aux, next_block) == -1) 
                    {
                        printf("SimpleFS_createFile: error while writing a block\n");
                        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
                        free(file); free(fh);
                        return NULL;
                    }
                    //printf("File %s created\n", fh->fcb->fcb.name);
                    //printf("File %s created\t[DB]\tblock in disk: %d\n", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

                    return fh;
				}
			}
			next_block = aux.header.next_block;
        } //eow
    }
    //arrivare qui significa non aver trovato un db libero dove mettere il file
    
    //ricalcolare new block perchè il prossimo blocco va al db
    int free_db = new_block;
    
    DirectoryBlock new_db = {0};
	new_db.header.next_block = -1;
	new_db.header.block_in_file = block_in_file + 1; //block in file e block in disk sono cose diverse, in file rappresenta l'i-esimo blocco dello stesso file, in disk è la posizione globale
	
    new_block = DiskDriver_getFreeBlock(disk, 0); //ricalcolo blocco in cui mettere il file
    if(new_block == -1)
    {
        printf("SimpleFS_createFile: error while asking for a free block_2\n");
        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
        return NULL;
    }
	new_db.file_blocks[0] = new_block; //lo metto nel primo dei blocchi del db
   
    //updating: il blocco che avevo scritto per il file ora va liberato e ci devo riscrivere la directory
    if(DiskDriver_freeBlock(disk, free_db) == -1) 
    {
		printf("SimpleFS_createFile: error while freeing a block\n");
		free(file); free(fh);
		return NULL;
	}
    //scrivo su disco in pos free_db
    if(DiskDriver_writeBlock(disk, &new_db, free_db) == -1) 
    {
		printf("SimpleFS_createFile: error while writing a block\n");
		free(file); free(fh);
		return NULL;
	}
    fdb->fcb.size_in_blocks += 1; fdb->fcb.size_in_bytes += BLOCK_SIZE; //ho creato un DB, aggiorno le sizes
    printf("New Directory Block created!\tblock in disk: %d\nPrevious block: %d\tNext block: %d\n", 
                                                                free_db, new_db.header.previous_block, new_db.header.next_block);

    //updating fh.block_in_disk
    fh->fcb->fcb.block_in_disk = new_block;
    if(DiskDriver_writeBlock(disk, file, new_block) == -1) //scrivo il blocco del file nel blocco nuovo calcolato
    {
		printf("SimpleFS_createFile: error while writing a block\n");
		free(file); free(fh);
		return NULL;
	}

    fdb->num_entries++; fdb->fcb.size_in_blocks++; fdb->fcb.size_in_bytes += BLOCK_SIZE; //aggiornamento directory control block
    
    //capire se attaccare new db a fdb o a aux, ultimo db visitato
    if(fdb->header.next_block == -1 ) //change pos in next block (fdb or db)
    {	
        fdb->header.next_block = free_db; //aggiorno il next di fdb
        //printf("fdb next header = %d\n", fdb->header.next_block);
        new_db.header.previous_block = fdb->fcb.block_in_disk; //blocco in disco del first directory block
        //printf("File %s created\n", fh->fcb->fcb.name);
		//printf("File %s created\t[FDB1]\tblock in disk: %d\t\t", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

        //printf("Previous block: %d\tNext block: %d\n\n", fh->fcb->header.previous_block, fh->fcb->header.next_block);

        return fh;        
	} 
    else
    {	//aux rappresenta l'ultima db visitata nella ricerca di un blocco libero
		aux.header.next_block = free_db; //aggiorno il next del db, mettendoci il db appena creato
        new_db.header.previous_block = block_number; //blocco in disco del first directory block

        //printf("File %s created\n", fh->fcb->fcb.name);
        //printf("File %s created\t[DB1]\tblock in disk: %d\n", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

        //printf("Previous block: %d\tNext block: %d\n", fh->fcb->header.previous_block, fh->fcb->header.next_block);
        return fh;			
	}
}


// opens a file in the  directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename)
{
    if(filename == NULL) 
    {
        printf("Invalid param (filename)");
        return NULL;
    }
    if(d == NULL) 
    {
        printf("Invalid param (dir_handle)");
        return NULL;
    }

    DiskDriver* disk = d->sfs->disk;
    FirstDirectoryBlock* fdb = d->dcb;
    
    if(fdb->num_entries > 0)
    {
        FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
        for(int i = 0; i < FDB_SPACE; i++)
        {
            if(fdb->file_blocks[i] > 0 && DiskDriver_readBlock(disk, ffb, fdb->file_blocks[i]) != -1)
            {
                if(strncmp(ffb->fcb.name,filename,128) == 0 )
                {
                    FileHandle* fh = (FileHandle*) malloc(sizeof(FileHandle)); //creazione fh di ritorno
                    fh->sfs = d->sfs;
                    fh->directory = fdb;
                    fh->pos_in_file = 0;
                    fh->sfs->disk = d->sfs->disk;
                    fh->fcb = ffb;
                    fh->current_block = &(ffb->header);
                    
                    return fh;
                }
            }
        }
        //si va in directory blocks ora
        int next = fdb->header.next_block;
        DirectoryBlock db;
        
        while(next != -1)
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
                        FileHandle* fh = (FileHandle*) malloc(sizeof(FileHandle)); //creazione fh di ritorno
                        fh->sfs = d->sfs;
                        fh->directory = fdb;
                        fh->pos_in_file = 0;
                        fh->sfs->disk = d->sfs->disk;
                        fh->fcb = ffb;
                        fh->current_block = &(ffb->header);
                       
                        return fh;
                    }                    
                }
            }
        next = db.header.next_block;
        }

        //se sono arrivato qui il file non esiste
        printf("SimpleFS_openFile: file does not exist\n");
        
        return NULL;
        
    }
    else
    {
        printf("SimpleFS_openFile: empty directory\n");
		return NULL;
    }
}

// closes a file handle (destroyes it)
int SimpleFS_close(FileHandle* f)
{
    //printf("Closing %s \n", f->fcb->fcb.name);

    //printf("Freeing fcb...");
    if(f->fcb != NULL) free(f->fcb);
    //else printf("Already free\t");
    //printf("Freeing structure...");
    if(f != NULL) free(f);
    //else printf("Already free\t");
    
    //printf("OK\n");
    return 0;
}

int SimpleFS_free_dir(DirectoryHandle* f)
{
    //if(f->current_block != NULL) free(f->current_block);
	if(f->dcb != NULL) free(f->dcb);
	//if(f->directory != NULL) free(f->directory);
	free(f);
	return 0;
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle* f, void* data, int size)
{
    //invalid param check
    if(f == NULL) 
    {
        printf("Invalid param (filehandle)");
        return -1;
    }
    if(data == NULL) 
    {
        printf("Invalid param (data)");
        return -1;
    }
    if(size < 0) 
    {
        printf("Invalid param (size)");
        return -1;
    }

    FirstFileBlock* fcb = f->fcb; //ffb extraction 
    DiskDriver* disk = f->sfs->disk; // disk extraction
    
    int written_bytes = 0;
    int offset = f->pos_in_file;
    int btw = size; //byte to write
    if(offset < FFB_SPACE)
    {   
        if(size <= FFB_SPACE - offset) //1 - best scenario: scrivo in ffb
        {
            memcpy(fcb->data + offset, (char*) data, size);
            
            written_bytes = size;
            //updating
            if(f->pos_in_file+written_bytes > fcb->fcb.written_bytes) fcb->fcb.written_bytes = f->pos_in_file+written_bytes; //nel caso avessi aggiunto byte scritti aggiorno
            
            if(DiskDriver_freeBlock(disk, fcb->fcb.block_in_disk) == -1)
            {
                printf("fs_wr error while updating (free)");
                return -1;
            }
            
            if(DiskDriver_writeBlock(disk, fcb, fcb->fcb.block_in_disk) == -1)
            {
                printf("fs_wr error while updating (write)");
                return -1;
            }

            return written_bytes; //chiudo
        } //BEST SCENARIO ENDS
        else //2 - offset ancora in ffb, ma i byte da scrivere non entrano tutti
        {
            memcpy(fcb->data+offset, (char*)data, FFB_SPACE - offset);
            written_bytes += FFB_SPACE - offset; //aggiorno written bytes
            btw = size - written_bytes; //aggiorno byte to write
            //DiskDriver_freeBlock(disk,fcb->fcb.block_in_disk);
            //DiskDriver_writeBlock(disk, fcb, fcb->fcb.block_in_disk);
            offset = 0; //offset a 0 perchè ora giungo ad un fileblock e si riinizia a scrivere
        }
    }
    else offset -= FFB_SPACE;//3 - pos fuori dall'ffb, bisogna proprio cambiare blocco
    /*
        se mi trovo qui è perchè non ho finito di scrivere, e ciò può essere capitato per due ragioni:
        1) l'offset era nell'ffb, ma dovevo scrivere più di quanto spazio avessi e quindi ora mi serve continuare nell'fb 
                                                                    (wr = FFB - offset && btw = size - wr  && offset = 0)
                ******OPPURE*******                                                    
        2) l'offset era già di suo fuori dall'ffb, dunque devo ancora iniziare a scrivere 
                                                                    (wr = 0 && btw == size && offset -= FFB_SPACE)
    */
   //printf("pffset >= FFB_SPACE\n");
    int next_block = fcb->header.next_block;

    int block_in_disk = fcb->fcb.block_in_disk;
	int block_in_file = fcb->header.block_in_file;
	FileBlock temp;

    while(written_bytes < size)
    {
        if(next_block == -1) //non ci sono blocchi, lo creiamo
        {
            FileBlock new_fb = {0}; //struttura utile per permettermi di riazzerare temp, una volta averla scritta su disco
		    new_fb.header.block_in_file = block_in_file + 1;
		    new_fb.header.next_block = -1;
		    new_fb.header.previous_block = block_in_disk;

    		next_block = DiskDriver_getFreeBlock(disk, block_in_disk);

            if(fcb->header.next_block == -1)
            {
                fcb->header.next_block = next_block; //aggiorno l'header di ffb mettendo il first free block partendo da block_in_disk, ovvero il blocco dell'fcb
			    DiskDriver_freeBlock(disk, block_in_disk); //updating fcb
			    DiskDriver_writeBlock(disk, fcb, block_in_disk); 
            }
            else //in questo else ci può entrare solo quando temp != NULL, perchè se ci entra alla prima iterazione del while al limite il blocco da ttaccare è all'ffb (chiamato qui fcb)
            {
                temp.header.next_block = next_block; //aggiorno l'header di fb mettendo il first free block partendo da block_in_disk, ovvero il blocco dell'fcb
			    DiskDriver_freeBlock(disk, block_in_disk); //updating fb
			    DiskDriver_writeBlock(disk,&temp, block_in_disk);
            }                           
            DiskDriver_writeBlock(disk, &new_fb, next_block); //scrivo il nuovo fileblock appena attaccato attaccato sul disco
            temp = new_fb; //azzero temp così da poterci scrivere

            fcb->fcb.size_in_blocks += 1; fcb->fcb.size_in_bytes += BLOCK_SIZE;
        }
        else if(DiskDriver_readBlock(disk, &temp, next_block) == -1) return -1; //se il next bloc esiste lo leggo e metto in tempo

        if(offset < FB_SPACE) //se ho raggiunto l'offset, perchè rientra in questo FileBlock
        {
            if(btw <= FB_SPACE - offset) //se i btw entrano tutti qui
            {
                memcpy(temp.data + offset, (char*)data, FB_SPACE - offset); //scrivo per il restante FB_SPACE - offset in data partendo dal nuovo offset
                written_bytes += btw; //aggiorno i byte scritti

                if(f->pos_in_file+written_bytes > fcb->fcb.written_bytes) fcb->fcb.written_bytes = f->pos_in_file+written_bytes; //nel caso avessi aggiunto byte, aggiorno
                if(DiskDriver_freeBlock(disk, fcb->fcb.block_in_disk) == -1)
                {
                    printf("fs_wr error while updating (free)");
                    return -1;
                }
                
                if(DiskDriver_writeBlock(disk, fcb, fcb->fcb.block_in_disk) == -1)
                {
                    printf("fs_wr error while updating (write)");
                    return -1;
                }
                if(DiskDriver_freeBlock(disk, next_block) == -1)
                {
                    printf("fs_wr error while updating (free)");
                    return -1;
                }
                
                if(DiskDriver_writeBlock(disk, &temp, next_block) == -1)
                {
                    printf("fs_wr error while updating (write)");
                    return -1;
                }

                return written_bytes;
            }
            else
            {
                memcpy(temp.data+offset,(char*)data + written_bytes, FB_SPACE - offset);
                written_bytes += FB_SPACE - offset;
                btw = size - written_bytes;
                //DiskDriver_freeBlock(disk, next_block);
                //DiskDriver_writeBlock(disk, &temp, next_block);
                offset = 0;
            }
        }
        else offset -= FB_SPACE; //se ancora non ho raggiunto la posizione giusta continuo a fare questa operazione

		block_in_disk = next_block;	// update index of current_block
		next_block = temp.header.next_block; // update index of next_block
		block_in_file = temp.header.block_in_file; // update index of file pos
    }//EOW
}


int SimpleFS_read(FileHandle* f, void* data, int size)
{
    //invalid param check
    if(f == NULL) 
    {
        printf("Invalid param (filehandle)");
        return -1;
    }
    if(data == NULL) 
    {
        printf("Invalid param (data)");
        return -1;
    }
    if(size < 0) 
    {
        printf("Invalid param (size)");
        return -1;
    }
    
    FirstFileBlock* ffb = f->fcb;
    DiskDriver* disk = f->sfs->disk;
    int offset = f->pos_in_file ;
    int bytes_read = 0;
    int readable = size;

    //memset(data, 0, size); 

    if(offset < FFB_SPACE && size <= FFB_SPACE - offset)
    {
        memcpy(data, ffb->data + offset, readable);
        f->pos_in_file += readable;
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
    if(f == NULL) 
    {
        printf("Invalid param (filehandle)");
        return -1;
    }
    if(pos < 0) 
    {
        printf("Invalid param (position)");
        return -1;
    }

    //int size = 0;
    FirstFileBlock* ffb = f->fcb;//(FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    
    if(pos > ffb->fcb.written_bytes)
    {
        free(ffb);
        printf("SimpleFS_seek: file's too short, returning -1\n");
        return -1;
    }
    f->pos_in_file = pos;
    return pos;
}

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle

//ogni volta che chiamo cd ho memory leak di 512 byte >:( *******
//questo perchè dovrei fare la free del first directory block a cui punta il directory handle prima di riassegnargli un puntatore, ma se faccio free esplode tutto e ho errore
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname)
{
    if(d == NULL) 
    {
        printf("Invalid param (dir_handle)");
        return -1;
    }
    if(dirname == NULL) 
    {
        printf("Invalid param (dirname)");
        return -1;
    }
    if (strcmp(d->dcb->fcb.name, dirname) == 0)
        return 0;
    
    if (strcmp(dirname, ".") == 0) 
        return 0;

    int ret = 0;
    DiskDriver* disk = d->sfs->disk;
    FirstDirectoryBlock* temp = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
    if(strcmp(dirname, "..") == 0)//torno a directory superiore, prior check per vedere se sono già la root
    {
        if(d->directory == NULL)
        {
            printf("SimpleFS_changeDir: we already in root directory, cannot proceed\n");
            return 0; //faccio ritornare 0 perchè in se per se quesdto non è un errore, è solo la gestione di un caso anomalo, accompagno con un messaggio che avvisa
        }
        
        int p_block = d->directory->fcb.block_in_disk;

        if(p_block == -1) 
        {
            d->directory = NULL;
            return 0;
        }

        ret = DiskDriver_readBlock(disk, temp, p_block);
        if(ret == -1)
        {
            printf("SimpleFS_changeDir: cannot read parent directory\n");
            return -1;
        }
        d->current_block =  &d->dcb->header;
        //free(d->dcb);
        d->dcb = temp;
        d->pos_in_block = 0;
        d->pos_in_dir = 0;
        if(temp->fcb.directory_block == -1) d->directory = NULL;
        else
        {
            FirstDirectoryBlock pp;
            ret = DiskDriver_readBlock(disk, &pp,temp->fcb.directory_block);
            d->directory = &pp;
        }
        //printf("Directory successfully changed, we in %s now\n", d->dcb->fcb.name);
        //printf("num entries: %d\nblock in disk: %d\tprevious block: %d\tnext block: %d\n", d->dcb->num_entries, d->dcb->fcb.block_in_disk, d->dcb->header.previous_block, d->dcb->header.next_block);
        //printf("Father directory block: %d\n", d->dcb->fcb.directory_block);
        temp = NULL;
        free(temp);
        return 0;
    }
    else if(d->dcb->num_entries <= 0)
    {
        printf("SimpleFS_changeDir: empty dir\n");
        free(temp);
        return -1;
    }
    //normal cd now-------------------------------------------------------------------------------------------------------
    FirstDirectoryBlock* fdb = d->dcb;
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    
    for(int i = 0; i < FDB_SPACE; i++)
    {
        if(fdb->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, temp, fdb->file_blocks[i])) != -1)
        {
            if(temp->fcb.is_dir)
            {
                if(strcmp(temp->fcb.name,dirname) == 0)
                {
                    d->directory = fdb;
                    d->current_block = &d->dcb->header;
                    //free(d->dcb);
                    d->dcb = temp;
                    d->pos_in_block = 0; 
                    d->pos_in_dir = i;
                
                    temp = NULL;
                    free(temp);
                    return 0;
                }
            }
        }
    }

    int next_block = fdb->header.next_block;
    int pos_in_block = 0;
    DirectoryBlock db;
    while(next_block != -1)
    {
        pos_in_block += 1;
        ret = DiskDriver_readBlock(disk, &db, next_block);
		if(ret == -1)
        {
			printf("SimpleFS_changeDir: cannot read the block\n");
            //free(temp);
			return -1;
		}

        for(int i = 0; i < DB_SPACE; i++)
        {
            if(db.file_blocks[i] > 0 && (DiskDriver_readBlock(disk, temp, db.file_blocks[i])) != -1)
            {
                if(temp->fcb.is_dir)
                {   
                    if(strcmp(temp->fcb.name,dirname) == 0)
                    {   
                        d->directory = fdb;
                        d->current_block = &d->dcb->header;
                        //free(d->dcb);
                        d->dcb = temp;
                        d->pos_in_block = 0; 
                        d->pos_in_dir = i;
                        
                        temp = NULL;
                        free(temp);
                        return 0;
                    }
                }
                
            }
        }
        next_block = db.header.next_block;
    }
    printf("SimpleFS_changeDir: cannot change directoryA\n");

    temp = NULL;
    free(temp);

    return -1;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname)
{
    if(d == NULL) 
    {
        printf("Invalid param (dir_handle)");
        return -1;
    }
    if(dirname == NULL) 
    {
        printf("Invalid param (dirname)");
        return -1;
    }
    DiskDriver* disk = d->sfs->disk;
    FirstDirectoryBlock* dcb = d->dcb;
    FirstDirectoryBlock* temp = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock)); 
    if(temp == NULL)
    {
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
    } // exists check passed

    if(disk->header->free_blocks < 1)
    {
        printf("Not enough space!\n");
        return -1;
    }
    free(temp);
    //dir creating___________________________________________________________________________________

    if(disk->header->free_blocks < 1)
    {
        printf("Not enough space!\n");
        return -1;
    }
    
    int nb = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
    if(nb == -1)
    {
        printf("SimpleFS_mkDir: error while asking disk for a free block\n");
        return -1;
    }
   
    FirstDirectoryBlock* new_dir = (FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
	if(new_dir == NULL)
    {
		printf("SimpleFS_mkDir: (new_dir) malloc error while creating FirstDirecotryBlock\n");
        free(new_dir);
		return -1;
	}
    memset(new_dir, 0, sizeof(FirstDirectoryBlock));
    
    new_dir->header.next_block = -1; 
    new_dir->header.previous_block = -1; 
    new_dir->header.block_in_file = 0;

    new_dir->fcb.directory_block = dcb->fcb.block_in_disk; 
    new_dir->fcb.block_in_disk = nb; 
    new_dir->fcb.is_dir = 1;
    new_dir->fcb.size_in_blocks = 1; 
    new_dir->fcb.size_in_bytes = BLOCK_SIZE; //effettivamente vado a scrivere un blocco, ha sensp
    new_dir->fcb.written_bytes = 0;
    new_dir->num_entries = 0;

    strcpy(new_dir->fcb.name, dirname);
   
    memset(new_dir->file_blocks, 0, FDB_SPACE); //setting data to 0
    //setup ended

    int block_number = dcb->fcb.block_in_disk;
    //scrivere su disco la directory

    int ret = DiskDriver_writeBlock(disk, new_dir, nb);
    if(ret == -1)
    {
        printf("SimpleFS_mkDir: cannot write new block on disk\n");
        free(new_dir);
        return -1;
    }

    //assegno il blocco alla directory

    int idx = 0;
    int block_in_file = 0;

    DirectoryBlock aux;

    if(dcb->num_entries < FDB_SPACE)
    {
        for(int i = 0; i < FDB_SPACE; i++)
        {
            if(dcb->file_blocks[i] == 0)
            {
                dcb->file_blocks[i] = nb;
                dcb->num_entries++; 
                dcb->fcb.size_in_blocks++; 
                dcb->fcb.size_in_bytes += BLOCK_SIZE;
		        
			    //update free then write
		        DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		        DiskDriver_writeBlock(disk, dcb, dcb->fcb.block_in_disk);

                //printf("Directory %s created\tblock in disk: %d\n", dirname, new_dir->fcb.block_in_disk);
                //printf("Previous block: %d\tNext block: %d\n\n", new_dir->header.previous_block, new_dir->header.next_block);

                free(new_dir);
                return 0; //<--------------------------
            }
        }
    }
    else
    {
        int next = dcb->header.next_block;
        
        while(next != -1)
        {
            ret = DiskDriver_readBlock(disk, &aux, next);
            if(ret == -1)
            {
                printf("SimpleFS_mkDir: cannot read next block from disk\n");
                free(new_dir);
                return -1;
            }
            block_in_file++;
            block_number = next;

            for(int i = 0; i < DB_SPACE; i++)
            {
                if(aux.file_blocks[i] == 0)
                {
                    idx = i;

                    aux.file_blocks[i] = nb;

                    dcb->num_entries++; dcb->fcb.size_in_blocks++; dcb->fcb.size_in_bytes += BLOCK_SIZE;
                    //DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		            //DiskDriver_writeBlock(disk, dcb, dcb->fcb.block_in_disk);
		            
                    if(DiskDriver_freeBlock(disk, block_number) == -1)
                    {
                        printf("SimpleFS_mkDir: error while freeing a block\n");
                        free(new_dir);
                        return -1;
                    }
		            if(DiskDriver_writeBlock(disk, &aux, block_number) == -1) 
                    {
                        printf("SimpleFS_createFile: error while writing a block\n");
                        free(new_dir);
                        return -1;
                    }

                    //printf("Directory %s created\tblock in disk: %d\n", dirname, new_dir->fcb.block_in_disk);

                    free(new_dir);
                    return 0;
                }
            }
			next = aux.header.next_block;
        }
    } 
    //bisogna riscrivere anche il fdb perchè prima va scritto il db della directory corrente, quindi...
    int nd_block = nb;

    DirectoryBlock new_db = {0};
	new_db.header.next_block = -1;
	new_db.header.block_in_file = block_in_file + 1;
	
    nb = DiskDriver_getFreeBlock(disk, 0);
	if(nd_block == -1)
    {
		printf("Cannot get a new free block: SimpleFS_mkDir\n");
		DiskDriver_freeBlock(disk, dcb->fcb.block_in_disk);
		return -1;
	}
    new_db.file_blocks[0] = nb;
    ret = DiskDriver_freeBlock(disk, nd_block); 
	if(ret == -1)
    {
		printf("Cannot free new_dir_block to create a new db: SimpleFS_mkDir\n" );
		free(new_dir);
		return -1;
	}

	ret = DiskDriver_writeBlock(disk, &new_db, nd_block); //scrivo il blocco su disco
	if(ret == -1)
    {
		printf("Cannot write new_dir_block on the disk: SimpleFS_mkDir\n" );
		free(new_dir);
		return -1;
	}
    dcb->fcb.size_in_blocks += 1;
    dcb->fcb.size_in_bytes += BLOCK_SIZE;
    //printf("New Directory Block created!\tblock in disk: %d\nPrevious block: %d\tNext block: %d\n", 
    //                                                            nd_block, new_db.header.previous_block, new_db.header.next_block);
    new_dir->fcb.block_in_disk = nb;
    if(DiskDriver_writeBlock(disk, new_dir, nb) == -1) //scrivo il blocco del file nel blocco nuovo calcolato
    {
		printf("SimpleFS_createFile: error while writing a block\n");
		free(new_dir);
		return -1;
	}

    dcb->num_entries++; dcb->fcb.size_in_blocks++; dcb->fcb.size_in_bytes += BLOCK_SIZE;

    if(dcb->header.next_block == -1)
    {
        dcb->header.next_block = nd_block;
        //printf("fdb next header is: %d\n", dcb->header.next_block);
        new_db.header.previous_block = dcb->fcb.block_in_disk;
        //printf("Directory %s created\tblock in disk: %d\n", dirname, new_dir->fcb.block_in_disk);
    } 
	else
    {
        aux.header.next_block = nd_block;
        new_db.header.previous_block = block_number;
        //printf("Directory %s created\tblock in disk: %d\n", dirname, new_dir->fcb.block_in_disk);
        
    } 
			
	//db = new_db;
	//block_number = nd_block; codice inutile
    free(new_dir);
	return 0;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, char* filename)
{
    if(d == NULL) 
    {
        printf("Invalid param (dirhandle)");
        return -1;
    }
    if(filename == NULL) 
    {
        printf("Invalid param (filename)");
        return -1;
    }
    DiskDriver* disk = d->sfs->disk;
    

    if(d->dcb->num_entries < 1)
    {
		printf("SimpleFS_remove: empty directory.\n");
		return -1;
	}

    FirstFileBlock ffb;

    int fdb_db = 0; //if 0 file was found in fdb else in db

    int fid = -1; //blocco del file
    int index = 0;
    int firstfound = 1; //significa che il file è stato trovato nel fdb

    for(int i = 0; i < FDB_SPACE; i++)
    {
        if(d->dcb->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, &ffb, d->dcb->file_blocks[i])) != -1)
        {
            if(strncmp(filename, ffb.fcb.name, strlen(filename)) == 0)
            {   //file/dir imputato trovato in fdb
                index = i; //utilizzato per poi riordinare tutti i blocchi
                fid = d->dcb->file_blocks[i];
                //fdb->file_blocks[i] = 0;
                break; //firstfound resta a 1, non entra nel while
            }
        }
    }
    
    DirectoryBlock db;
    int next = d->dcb->header.next_block;
    
	int block_in_disk = d->dcb->fcb.block_in_disk; //da qui possibile ottenere block in file
    int block_in_file = 0;

    while(fid == -1 && next != -1) //fid = -1 significa che non sta nell'fdb
    {
            block_in_file++; //aggiorno il blocco file, not sure im gonna use this variable
            firstfound = 0;
            int ret = DiskDriver_readBlock(disk, &db, next);
            if(ret == -1)
            {
                printf("SimpleFS_remove: cannot read block\n");
		        return -1;
            }
            for(int i = 0; i < DB_SPACE; i++)
            {
                if(db.file_blocks[i] > 0 && (DiskDriver_readBlock(disk, &ffb, db.file_blocks[i])) != -1)
                {
                    if(strncmp(filename, ffb.fcb.name, strlen(filename)) == 0)
                    {
                        index = i;
                        fid = db.file_blocks[i];//file/dir imputato in db
                        //db.file_blocks[i] = 0;
                        //fdb_db = 1;
                        
                        break;
                    }
                }
            }
        block_in_disk = next; //il blocco nel disco è l'ultimo next visitato
        next = db.header.next_block;       //aggiorno next
    } //EOW
    if(fid == -1)
    {
        printf("File does not exist!\n");
        return -1;
    }    

    FirstFileBlock* kill = (FirstFileBlock*) malloc(sizeof(FirstFileBlock)); //struttura da killare
    if(DiskDriver_readBlock(disk, kill, fid) == -1)
    {
        printf("SimpleFS_remove: cannot read file to kill\n");
        return -1;
    }   
    //in kill c'è il blocco imputato
    if(!kill->fcb.is_dir) //eliminazioe file
    {
        FileBlock aux;
        int next_block = kill->header.next_block; //next fileblock di kill
        int block;
        while(next_block != -1)
        {
            if(DiskDriver_readBlock(disk, &aux, next_block) == -1) return -1; //impossibile leggere successivi fileblock
			block = next_block;
			next_block = aux.header.next_block; //prendo il next dell'fb prima di eliminarlo
			if(DiskDriver_freeBlock(disk, block) == -1)
            {
                printf("SimpleFS_remove: cannot free the fileblock\n");
		        return -1;
            }
            aux.header.next_block = -1; aux.header.previous_block = -1;
            d->dcb->fcb.size_in_blocks -= 1; d->dcb->fcb.size_in_bytes -= BLOCK_SIZE; //aggiorno fcb sottrando le dimensioni dei fileblocks
        }
        kill->header.next_block = -1;
        if(DiskDriver_freeBlock(disk, fid) == -1) //eliminazioe vera e propria
        {
            printf("SimpleFS_remove: cannot free the firstfileblock\n");
		    return -1;
        }
        //@@@@@@@@@@@@@@@@UPDATING NOW@@@@@@@@@@@@@@@@@@@@@@@@@@@
        d->dcb->fcb.size_in_blocks -= 1; d->dcb->fcb.size_in_bytes -= BLOCK_SIZE; //aggiorno fcb sottrando le dimensioni dell'ffb
    }
    else //eliminazione cartella e file all'interno
    {   
        FirstDirectoryBlock* fdb_kill = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
        if(DiskDriver_readBlock(disk, fdb_kill, fid) == -1)
        {
            printf("SimpleFS_remove: cannot read dir to kill\n");
            return -1;
        } 
        
        if(fdb_kill->num_entries >= 1) //cartella !vuota (se la cartella è vuota skippa tutto questo e rimuove solo il blocco fid)
        {

            if(SimpleFS_changeDir(d, fdb_kill->fcb.name) == -1)
            {
                printf("cannot jump into dir to delete files\n");
                return -1;
            }

            for(int i = 0; i < FDB_SPACE; i++)
            {   //chiamata ricorsiva
                FirstFileBlock file;
                if(fdb_kill->file_blocks[i] > 0 && DiskDriver_readBlock(disk, &file, fdb_kill->file_blocks[i]) != -1) 
                {
                    SimpleFS_remove(d,file.fcb.name);
                }
            }

            int next_block = fdb_kill->header.next_block;
            int block = fid;
            DirectoryBlock db_aux;
            while(next != -1)
            {
                if(DiskDriver_readBlock(disk, &db_aux, next_block) == -1)
                {
                    printf("SimpleFS_remove: cannot read dirblock\n");
		            return -1;
                }
                for(int i = 0; i < DB_SPACE; i++)
                {
					FirstFileBlock ffb_rec;

					if(DiskDriver_readBlock(disk, &ffb_rec, db_aux.file_blocks[i]) == -1)
                    {
                        printf("SimpleFS_remove: cannot read ffb_rec\n");
                        return -1;
                    } 
					SimpleFS_remove(d, ffb_rec.fcb.name);
				}
                d->dcb->fcb.size_in_blocks -= 1; d->dcb->fcb.size_in_bytes -= BLOCK_SIZE;

				block = next_block;
				next_block = db_aux.header.next_block;
				DiskDriver_freeBlock(disk,block); //rimozione blocchi db
            }
            d->dcb->fcb.size_in_blocks -= 1; d->dcb->fcb.size_in_bytes -= BLOCK_SIZE;
            if(SimpleFS_changeDir(d, "..") == -1) //me ne ritorno nella mia cartella 
            {
                printf("cannot jump into dir to delete files\n");
                return -1;
            }
        }
        
        if(DiskDriver_freeBlock(disk, fid) == -1) //rimuovo blocco fdb
        {
            printf("SimpleFS_remove: cannot free the block\n");
		    return -1;
        }
        d->dcb->fcb.size_in_blocks -= 1; d->dcb->fcb.size_in_bytes -= BLOCK_SIZE;
        free(fdb_kill);
    }
    free(kill); //liberare memoria allocata per l'ffb

    d->dcb->num_entries-=1;    
    
    if(!firstfound)
    {   
        //FirstDirectoryBlock* fdb = d->dcb;
        db.file_blocks[index] = 0;
        int check = 1;
        for(int i = 0; i < DB_SPACE; i++)
        {
            if(db.file_blocks[i] != 0) check = 0; //se il db è vuoto tanto vale rimuoverlo
        }

        int ret_1 = DiskDriver_freeBlock(d->sfs->disk, block_in_disk); //updating fdb n db on disk
        if(ret_1 != 0)
        {
            printf("Error while updating db");
            return -1;
        }
        if(!check)
        {
            int ret_2 = DiskDriver_writeBlock(d->sfs->disk, &db, block_in_disk);
            if(ret_2 != BLOCK_SIZE)
            {
                printf("Error while updating db");
                return -1;
            }
        }
        else 
        {
            //printf("db removed, block: %d\n", block_in_disk);
            d->dcb->header.next_block = -1;
        }
        
        /*int ret_3 = DiskDriver_freeBlock(d->sfs->disk, d->dcb->fcb.block_in_disk);
		int ret_4 = DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->fcb.block_in_disk);
        if(ret_3 != 0 || ret_4 != BLOCK_SIZE)
        {
            printf("Error while updating fdb");
            return -1;
        }*/
		return 0;
    }
    else
    {
        d->dcb->file_blocks[index] = 0;
        FirstDirectoryBlock* fdb = d->dcb;
        int ret_1 = DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk); //updating fdb on disk
		int ret_2 = DiskDriver_writeBlock(disk, fdb, fdb->fcb.block_in_disk);

        if(ret_1 != 0 || ret_2 != BLOCK_SIZE)
        {
            printf("Error while updating fdb");
            return -1;
        }
		return 0;
    }
    printf("file not found, returning -1\n");
    return -1;
}

