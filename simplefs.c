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

// reads in the (preallocated) blocks array, the name of all files in a directory 
int SimpleFS_readDir(char** names, DirectoryHandle* d, int* flag)
{
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

    int count = 0;

    FirstDirectoryBlock* dcb = d->dcb;
    DiskDriver* disk = d->sfs->disk;

    if(dcb->num_entries == 0)
    {
        printf("Empty directory\n");
        return 0;
    }

    if(dcb->num_entries < 0)
    {
        printf("SimpleFS_readDir: negative entries num!!!\n");
        return -1;
    }

    FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    if(ffb == NULL)
    {
		printf("SimpleFS_readDir: malloc error on db\n");
		return -1;
	}
    
    int* blocks = dcb->file_blocks; //primo blocco directory, da scorrere tutto
    for(int i = 0; i < FDB_SPACE; i++)
    {
       // printf("\nBlock num.:\t%d\n", blocks[i]);
       
        if(blocks[i] > 0 && DiskDriver_readBlock(disk, ffb, blocks[i]) != -1)
        {
            strncpy(names[count], ffb->fcb.name, 128); 
            flag[count] = ffb->fcb.is_dir; //salvo nella corrispondente pos il flag
            count++;
            
            
        }
    }

    int next = dcb->header.next_block;
	DirectoryBlock db;

	while (next != -1) //next check
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
    
    free(ffb);
    return 0; //aight
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

    int new_block = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
    if(new_block == -1)
    {
        printf("SimpleFS_createFile: error while asking for a free block\n");
        DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
        return NULL;
    }
    //firstfileblock
    //setup file
    FirstFileBlock* file = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    memset(file, 0, sizeof(FirstFileBlock)); //cleared, così valgrind non piange

    file->header.next_block = -1; file->header.previous_block = -1; file->header.block_in_file = 0; //default values
    
    strcpy(file->fcb.name, filename); 

    file->fcb.directory_block = fdb->fcb.block_in_disk;
    file->fcb.block_in_disk = new_block;
    file->fcb.is_dir = 0;
    file->fcb.size_in_blocks = 1;
    file->fcb.size_in_bytes = BLOCK_SIZE;
    file->fcb.written_bytes = 0;

    //setup filehandle
    FileHandle* fh = malloc(sizeof(FileHandle));
	fh->sfs = d->sfs;
	fh->fcb = file;
	fh->directory = fdb;
	fh->pos_in_file = 0;
    fh->current_block = &(fh->fcb->header);

    //-----------------------------set up ended---------------------------------------

    //ok now we put the block on the disk

    int ret = DiskDriver_writeBlock(disk, file, new_block);
    if(ret == -1)
    {
        printf("SimpleFS_createFile: error while writing block on the disk\n");
        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
		return NULL;
    }
    //blocco scritto, tocca assegnarlo alla directory, capendo a quale blocco deve attaccarsi

    //int found = 0;
	int block_entry = 0; //for tracking
	//int no_space_fdb = 0;
	int block_number = fdb->fcb.block_in_disk;
	int block_in_file = 0;

    DirectoryBlock aux;

    if(fdb->num_entries < FDB_SPACE) //c'è spazio nell'fdb
    {
        int* blocks = fdb->file_blocks;
        for(int j = 0; j < FDB_SPACE; j++)
        {
			if(blocks[j] == 0) //blocco libero, c'è spazio
            {  
				//found = 1;
				block_entry = j;

                fh->fcb->header.previous_block = blocks[block_entry - 1];
                fdb->file_blocks[block_entry] = new_block;
                fdb->num_entries++; fdb->fcb.size_in_blocks++; fdb->fcb.size_in_bytes += BLOCK_SIZE;

                //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk); //update
		        //DiskDriver_writeBlock(disk, fdb, fdb->fcb.block_in_disk);
                //printf("File %s created\n", fh->fcb->fcb.name);
                printf("File %s created\t[FDB]\tblock in disk: %d\t\t", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

                printf("Previous block: %d\tNext block: %d\n\n", fh->fcb->header.previous_block, fh->fcb->header.next_block);

                return fh;
			}
        }        
    }
    else
    { //provo nel directory block
        //printf("Free block not found in fdb, going to next blocks\n");

        //no_space_fdb = 1;

        int next_block = fdb->header.next_block;

        while(next_block != -1 ) //finche c'è un next db e finche non ho trovato uno spazio
        { 
			ret = DiskDriver_readBlock(disk,&aux,next_block); //leggo 
			if(ret == -1)
            {
				printf("SimpleFS_createFile: error while reading blocks (DB)\n");
				//DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
				return NULL;
			}//leggo come prima
            int* blocks = aux.file_blocks;		//cerco nello stesso modo di prima
			block_number = next_block;
			block_in_file++;

			for(int i = 0; i < DB_SPACE;i++)
            {
				if(blocks[i] == 0)
                {printf("\n");
					//found = 1;
					block_entry = i;
                    
                    //fdb->file_blocks[block_entry] = new_block;
                    aux.file_blocks[block_entry] = new_block;
                    fh->fcb->header.previous_block = aux.file_blocks[block_entry - 1];
                    
                    fdb->num_entries++; fdb->fcb.size_in_blocks++; fdb->fcb.size_in_bytes += BLOCK_SIZE;
                    //updating directory block
                    if(DiskDriver_freeBlock(disk, next_block) == -1) 
                    {
                        printf("SimpleFS_createFile: error while writing a block\n");
                        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
                        return NULL;
                    }

                    //scrivo su disco in pos free_db
                    if(DiskDriver_writeBlock(disk, &aux, next_block) == -1) 
                    {
                        printf("SimpleFS_createFile: error while writing a block\n");
                        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
                        return NULL;
                    }

                    //printf("File %s created\n", fh->fcb->fcb.name);
                    printf("File %s created\t[DB]\tblock in disk: %d\n", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

                    printf("Previous block: %d\tNext block: %d\n", fh->fcb->header.previous_block, fh->fcb->header.next_block);
                    return fh;
				}
			}

			next_block = aux.header.next_block;
        } //eow
    }
    //ricalcolare new block perchè il prossimo blocco va al db
    int free_db = new_block;
    if(free_db == -1)
    {
        printf("SimpleFS_createFile: error while asking for a free block_2\n");
        //DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
        return NULL;
    }

    DirectoryBlock new_db = {0};
	new_db.header.next_block = -1;
	new_db.header.block_in_file = block_in_file;
	new_db.header.previous_block = block_number;

    new_block = DiskDriver_getFreeBlock(disk, 0); //ricalcolo blocco in cui mettere il file

	new_db.file_blocks[0] = new_block; //lo metto nel primo dei blocchi del db
   
    //updating: il blocco che avevo scritto per il file ora va liberato e ci devo riscrivere la directory
    if(DiskDriver_freeBlock(disk, free_db) == -1) 
    {
		printf("SimpleFS_createFile: error while writing a block\n");
		//DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
		return NULL;
	}
    //scrivo su disco in pos free_db
    if(DiskDriver_writeBlock(disk, &new_db, free_db) == -1) 
    {
		printf("SimpleFS_createFile: error while writing a block\n");
		//DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
		return NULL;
	}

    printf("New Directory Block created!\tblock in disk: %d\nPrevious block: %d\tNext block: %d\n", 
                                                                free_db, new_db.header.previous_block, new_db.header.next_block);

    //updating fh.block_in_disk e il previous block
    
    fh->fcb->fcb.block_in_disk = new_block;
    if(DiskDriver_writeBlock(disk, file, new_block) == -1) //scrivo il blocco del file nel blocco nuovo calcolato
    {
		printf("SimpleFS_createFile: error while writing a block\n");
		//DiskDriver_freeBlock(disk,fdb->fcb.block_in_disk);
		return NULL;
	}

    fdb->num_entries++; fdb->fcb.size_in_blocks++; fdb->fcb.size_in_bytes += BLOCK_SIZE; //aggiornamento directory control block
    
    //capire se attaccare new db a fdb o a aux, ultimo db visitato
    if(fdb->header.next_block == -1 ) //change pos in next block (fdb or db)
    {	
        fdb->header.next_block = free_db; //aggiorno il next di fdb
        printf("fdb next header = %d\n", fdb->header.next_block);
        fh->fcb->header.previous_block = free_db;

        //printf("File %s created\n", fh->fcb->fcb.name);
		printf("File %s created\t[FDB1]\tblock in disk: %d\t\t", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

        printf("Previous block: %d\tNext block: %d\n\n", fh->fcb->header.previous_block, fh->fcb->header.next_block);

        return fh;        
	} 
    else
    {	//aux rappresenta l'ultima db visitata nella ricerca di un blocco libero
		aux.header.next_block = free_db; //aggiorno il next del db, mettendoci il db appena creato
        //aux.file_blocks[block_entry] = new_block;

        fh->fcb->header.previous_block = free_db; //il blocco precedente del mio file è il direcotry block
        
        //printf("File %s created\n", fh->fcb->fcb.name);
        printf("File %s created\t[DB1]\tblock in disk: %d\n", fh->fcb->fcb.name, fh->fcb->fcb.block_in_disk);

        printf("Previous block: %d\tNext block: %d\n", fh->fcb->header.previous_block, fh->fcb->header.next_block);
        return fh;			
	}
    /*[NB]:
        Ricordarsi che nel momento in cui si crea un nuovo db, il block_in_disk del successivo file creato, che appunto andrà a finire nel nuovo db,
        sara +1 maggiore di quello che ci si aspetta, questo perchè il db stesso è un blocco. 
        Nell'esempio del test si satura il disco con 98 file e 2 blocchi directory: l'fdb con blocco zero, il db poi, che ha blocco in disco pari ad 87,
        avrà come previous block il blocco 0, ovvero quello dell'fdb.
    */
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
    printf("Closing %s \n", f->fcb->fcb.name);

    printf("Freeing fcb...");
    if(f->fcb != NULL) free(f->fcb);
    else printf("Already free\t");
    printf("Freeing structure...");
    if(f != NULL) free(f);
    else printf("Already free\t");
   
    
    printf("OK\n");
    
    return 0;
}

int SimpleFS_free_dir(DirectoryHandle* f)
{
    //if(f->current_block != NULL) free(f->current_block);
	if(f->dcb != NULL) free(f->dcb);
	if(f->directory != NULL) free(f->directory);
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
    //printf("OK1\n");
    if(offset < FFB_SPACE)
    {   //printf("offset < FFB_SPACE\n");
        if(size <= FFB_SPACE - offset) //1 - best scenario: scrivo in ffb
        {
            memcpy(fcb->data + offset, (char*) data, size);
            
            written_bytes = size;
            //updating
            if(f->pos_in_file+written_bytes > fcb->fcb.written_bytes) fcb->fcb.written_bytes = f->pos_in_file+written_bytes; //nel caso avessi aggiunto byte scritti aggiorno
            
            //DiskDriver_freeBlock(disk, fcb->fcb.block_in_disk);
            
            //DiskDriver_writeBlock(disk, fcb, fcb->fcb.block_in_disk);//flush inside
            //printf("OK2\n");
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

    		next_block = DiskDriver_getFreeBlock(disk,block_in_disk);

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
        }
        else if(DiskDriver_readBlock(disk, &temp, next_block) == -1) return -1; //se il next bloc esiste lo leggo e metto in tempo

        if(offset < FB_SPACE) //se ho raggiunto l'offset, perchè rientra in questo FileBlock
        {
            if(btw <= FB_SPACE - offset) //se i btw entrano tutti qui
            {
                memcpy(temp.data + offset, (char*)data, FB_SPACE - offset); //scrivo per il restante FB_SPACE - offset in data partendo dal nuovo offset
                written_bytes += btw; //aggiorno i byte scritti

                if(f->pos_in_file+written_bytes > fcb->fcb.written_bytes) fcb->fcb.written_bytes = f->pos_in_file+written_bytes; //nel caso avessi aggiunto byte, aggiorno

                DiskDriver_freeBlock(disk, next_block);
                DiskDriver_writeBlock(disk, &temp, next_block); //updating next block

                return written_bytes;
            }
            else
            {
                memcpy(temp.data+offset,(char*)data + written_bytes, FB_SPACE - offset);
                written_bytes += FB_SPACE - offset;
                btw = size - written_bytes;
                DiskDriver_freeBlock(disk, next_block);
                DiskDriver_writeBlock(disk, &temp, next_block);
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
            free(p);
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
            if(strcmp(temp->fcb.name,dirname) == 0)
            {
				//DiskDriver_readBlock(disk,temp,fdb->file_blocks[i]);
                if(!temp->fcb.is_dir)
                {
                    printf("%s is not a directory!\n", temp->fcb.name);
                    return -1;
                }
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
                if(strcmp(temp->fcb.name,dirname) == 0)
                {
					//DiskDriver_readBlock(disk, temp, db.file_blocks[i]);
                    if(!temp->fcb.is_dir)
                    {
                        printf("%s is not a directory!\n", temp->fcb.name);
                        return -1;
                    }
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
    } //already exists check passed

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
		return -1;
	}
    memset(new_dir, 0, sizeof(FirstDirectoryBlock));

    new_dir->header.next_block = -1; new_dir->header.previous_block = -1; new_dir->header.block_in_file = 0;

    new_dir->fcb.directory_block = dcb->fcb.block_in_disk; 
    new_dir->fcb.block_in_disk = nb; 
    new_dir->fcb.is_dir = 1;
    new_dir->fcb.size_in_blocks = 0; 
    new_dir->fcb.size_in_bytes = 0; 
    new_dir->fcb.written_bytes = 0;
    new_dir->num_entries = 0;
    
	strcpy(new_dir->fcb.name, dirname);

    memset(new_dir->file_blocks, 0, FDB_SPACE);
    

    int block_number = dcb->fcb.block_in_disk;
    //scrivere su disco la directory

    DirectoryBlock db;

    int ret = DiskDriver_writeBlock(disk, new_dir, nb);
    if(ret == -1)
    {
        printf("SimpleFS_mkDir: cannot write new block on disk\n");
        return -1;
    }

    free(new_dir);

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
    FirstDirectoryBlock* fdb = d->dcb;

    if(fdb->num_entries < 1)
    {
		printf("SimpleFS_remove: empty directory.\n");
		return -1;
	}

    FirstFileBlock ffb;
    int id = -1;
    int file_index;

    for(int i = 0; i < FDB_SPACE; i++)
    {
        if(fdb->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, &ffb, fdb->file_blocks[i])) != -1)
        {
            if(strcmp(filename, ffb.fcb.name) == 0)
            {   
                id = i;
                file_index = fdb->file_blocks[id];
                break;
            }
        }
    }
    //printf("\nmiao\n");
    int firstfound = 1; //significa che il file è stato trovato nel fdb

    DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));

    int next = fdb->header.next_block;
	int block_in_disk = fdb->header.block_in_file;

    while(id == -1 && next != -1) //id = -1 significa che non sta nell'fdb
    {
     
            firstfound = 0;
            int ret = DiskDriver_readBlock(disk, &db, next);
            if(ret == -1)
            {
                printf("SimpleFS_remove: cannot read block\n");
                //free(db);
		        return -1;
            }

            for(int i = 0; i < DB_SPACE; i++)
            {
                if(db->file_blocks[i] > 0 && (DiskDriver_readBlock(disk, &ffb, db->file_blocks[i])) != -1)
                {
                    if(strcmp(filename, ffb.fcb.name) == 0)
                    {
                        id = i;
                        file_index = db->file_blocks[id];
                        break;
                    }
                }
            }
        block_in_disk = next;
        next = db->header.next_block;       
    }
    if(id == -1)
    {
        printf("Filename does not exist!\n");
        free(db);
        return -1;
    }    

    FirstFileBlock* kill = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    if(DiskDriver_readBlock(disk, kill, file_index) == -1)
    {
        printf("SimpleFS_remove: cannot read file to kill\n");
        free(db);
        return -1;
    } 

    if(!kill->fcb.is_dir) //eliminazioe file
    {
        FileBlock aux;
        int next_block = kill->header.next_block; //prima era ridefinito
        int block = file_index;

        while(next_block != -1)
        {
            if(DiskDriver_readBlock(disk, &aux, next_block) == -1) return -1;
			block = next_block;
			next_block = aux.header.next_block;
			DiskDriver_freeBlock(disk, block);
        }
        if(DiskDriver_freeBlock(disk, file_index) == -1)
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
        if(DiskDriver_readBlock(disk, &fdb_kill, file_index) == -1)
        {
            printf("SimpleFS_remove: cannot read dir to kill\n");
            free(db);
            return -1;
        } 

        if(fdb_kill.num_entries < 1) //cartella vuota
        {
            if(DiskDriver_freeBlock(disk, file_index) == -1) //rimuovo solo blocco cartella
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
                FirstFileBlock file;
                if(fdb_kill.file_blocks[i] > 0 && DiskDriver_readBlock(disk,&file, fdb_kill.file_blocks[i]) != -1) SimpleFS_remove(d,file.fcb.name);
            }

            int next_block = fdb_kill.header.next_block;
            int block = file_index;
            DirectoryBlock db_aux;
            while(next != -1)
            {
                if(DiskDriver_readBlock(disk, &db_aux, next_block) == -1)
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
				block = next_block;
				next_block = db_aux.header.next_block;
				DiskDriver_freeBlock(disk,block);
            }
            DiskDriver_freeBlock(disk, file_index);
			d->dcb = fdb;
        }

    }
    free(kill);
    if(!firstfound)
    {
        	db->file_blocks[id] = 0;
			fdb->num_entries -= 1;
      
            DiskDriver_freeBlock(d->sfs->disk, block_in_disk);
			DiskDriver_writeBlock(d->sfs->disk, db, block_in_disk);
            DiskDriver_freeBlock(d->sfs->disk, fdb->fcb.block_in_disk);
			DiskDriver_writeBlock(d->sfs->disk, fdb, fdb->fcb.block_in_disk);

			return 0; //return 0;
    }
    else
    {
        fdb->file_blocks[id] = 0;
		fdb->num_entries-=1;
     
        DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
		DiskDriver_writeBlock(disk, fdb, fdb->fcb.block_in_disk);
		return 0;
    }
    printf("Couldn't remove filename, returning -1\n");
    return -1;
}

