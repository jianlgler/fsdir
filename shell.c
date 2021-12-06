#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h> 

#include "shell.h"

SimpleFS* fs;
DiskDriver disk;
DirectoryHandle* cur;

const char* filename = "./sfs1.txt";

DirectoryHandle* FS_setup(const char* filename, DiskDriver* disk, SimpleFS* simple_fs)
{
    DiskDriver_init(disk, filename, 8192);
    DiskDriver_flush(disk); //??

    DirectoryHandle* dh = SimpleFS_init(simple_fs,disk);

    printf("Filesystem initialized\n\n");
    return dh;
}

void FS_abortion(int dummy)
{
    printf("\nClosing...\n"); 
    if(cur != NULL) SimpleFS_free_dir(cur);
    if(fs != NULL) free(fs);
    printf("\tGoodbye\n");
    exit(EXIT_SUCCESS); 
}

void FS_status()
{
    printf("Filename: %s\n\n", filename);

    DiskDriver_print(&disk);
}

void FS_help()
{   //wrrd fast readwrite
    printf("\n");
    printf("Use mkdir <directory_name> to create new directory\n");
    printf("Use wr <file_name> to write inside a file\n");
    printf("Use rd <file_name> to read from a it\n");
    printf("Use write <file_name> to write inside a file at selected position\n");
    printf("Use read <file_name> to read from a file, starting from a selected position\n");
    printf("Use touch <file_name> to create an empty file\n");
    printf("Use ls to view a list of the files and folders in current directory \n");
    printf("Use cd <directory_name> to move into the selected directory\n");
    printf("Use rm <file or directory name> to remove a file or a directory with its content\n");
    printf("Use status to show disk informations\n");
    printf("Use quit or CTRL + C to exit the program\n\n");
    printf("Use help to show a list of the avaible commands (it repeats current messages)\n");
    return;
}

void FS_rm(int argc, char* argv[2])
{
    if(argc != 2)
    {
        printf("rm_error: you must use 'rm <file or directory name>' \n");
        return;
    }

    if(SimpleFS_remove(cur, argv[1]) == -1) printf("rm_error: cannot remove %s \n", argv[1]);
    printf("[%s removed successfully!]\n", argv[1]);
}

void FS_mkdir(int argc, char* argv[2])
{
    if(argc != 2)
    {
        printf("mkDir_error: you must use 'mkdir <directory_name>' \n");
        return;
    } 

    if (SimpleFS_mkDir(cur, argv[1]) == -1) printf("mkDir_error: cannot create directory %s\n", argv[1]);
}

void FS_touch(int argc, char* argv[2])
{
    if(argc != 2)
    {
        printf("touch_error: you must use 'touch <file_name>' \n");
        return;
    } 
    FileHandle* fh = SimpleFS_createFile(cur, argv[1]);
    if(fh == NULL) 
    {
        printf("touch_error: cannot create file %s\n", argv[1]);
        return;
    }
    else if(SimpleFS_close(fh) == -1)
    {
        printf("mem_mng error\n");
        return;
    }
}

void FS_cd(int argc, char* argv[2])
{
    if(argc != 2)
    {
        printf("cd_error: you must use 'cd <directory_name>' \n");
        return;
    } 
    if(SimpleFS_changeDir(cur, argv[1]) == -1)
    { 
        printf("cd_error: cannot change directory\n");
		return;
	}
}

void FS_ls()
{
    int* flags = (int*)malloc((cur->dcb->num_entries) * sizeof(int)); //flag per vedere se il determinato file is_dir
	if(flags == NULL)
    {
		printf("ls_error in readDirectory: malloc of flag.\n");
        free(flags);
		return;
	}
	for (int i = 0; i < cur->dcb->num_entries; i++) flags[i] = -1;
	
	char** names = (char**)malloc((cur->dcb->num_entries) * sizeof(char*)); //array di stringhe
	if(names == NULL)
    {
		printf("ls_error in readDirectory: malloc of names.\n");
        free(flags);
        free(names);
		return;
	}
    //non alloco perchè in readdir uso strdup
	//for (int i = 0; i < (cur->dcb->num_entries); i++) names[i] = (char*)malloc(128*sizeof(char)); //inizializzo ogni stringa

	//printf("[SHELLTEST]\n");
    if(SimpleFS_readDir(names, cur, flags) == -1) //lettura e inizializzazione array di interesse
    {
        printf("ls_error: could not use readDir\n");
        free(flags);
        free(names);
        return;
    }

    printf("content of %s:\n\n",cur->dcb->fcb.name);
    printf("Number of entries: %d\n\n", cur->dcb->num_entries);

    for (int i = 0; i < cur->dcb->num_entries; i++) 
    {
        if(flags[i] == 1) printf("\033[1;34m%s\033[0m\t", names[i]);//Directory testo directory in azzurro
        if(flags[i] == 0) printf("%s\t", names[i]);                  //File, white
        //dovrebbe andare bene anche così nel test faccio un altro ciclo for
        free(names[i]); //deallocate ater printing
    }

    free(names);
    free(flags);
    printf("\n");
}

void FS_wr(int argc, char* argv[2], int start_pos)
{
    if(start_pos < 0)
    {
        printf("wr_error: wrong starting position \n");
        return;
    } 
    
    FileHandle* fh = SimpleFS_openFile(cur, argv[1]); //apro file
    if(fh == NULL)
    {
        printf("wr_error: cannot open %s \n", argv[1]);
        return;
    }

    if(SimpleFS_seek(fh, start_pos) == -1)
    {
        printf("wr_error: cannot jump to starting position \n");
        SimpleFS_close(fh);
        return;
    }

    printf("\nInsert text:\n>\t");
    /*char* t = (char*) malloc(sizeof(BLOCK_SIZE));
    fgets(t, BLOCK_SIZE, stdin);*/
    char* t;
    int ch;
    size_t len = 0;
    t = realloc(NULL, sizeof(*t)*BLOCK_SIZE);//size is start size (= BLOCK SIZE)
    //if(!t) return;

    while(EOF!=(ch=fgetc(stdin)) && ch != '\n'){
        t[len++]=ch;
        if(len == BLOCK_SIZE){
            t = realloc(t, sizeof(*t)*(len + BLOCK_SIZE));
            //if(!t) return;
        }
    }
    t[len++]='\0';

    t = realloc(t, sizeof(*t)*len);


    if (SimpleFS_write(fh, t, strlen(t)) == -1) 
    {
        printf("wr_error: error while writeing\n");
        free(t);
        SimpleFS_close(fh);
        return;
    }

    //t = NULL;
    free(t);
    SimpleFS_close(fh);
}

void FS_rd(int argc, char* argv[2], int start_pos)
{
    if(argc != 2)
    {
        printf("rd_error: you must use 'rd <file_name>' \n");
        return;
    } 

    FileHandle* fh = SimpleFS_openFile(cur, argv[1]);
    if(fh == NULL)
    {
        printf("rd_error: cannot open %s \n", argv[1]);
        return;
    } 
    //printf("fh written bytes: %d\n", fh->fcb->fcb.written_bytes);
    if(start_pos < 0 || start_pos >= fh->fcb->fcb.written_bytes)
    {
        printf("rd_error: wrong starting position \n");
        return;
    } 
    if(SimpleFS_seek(fh, start_pos) == -1)
    {
        printf("rd_error: cannot jump to starting position \n");
        SimpleFS_close(fh);
        return;
    }
    //mi calcolo la lunghezza massima da dover leggere

    char* text = (char*) malloc(fh->fcb->fcb.written_bytes - start_pos);
    if(text == NULL)
    {
	    printf("rd_error: cannot allocate memory for text\n");
		SimpleFS_close(fh);
		return;
	}
    
    int size = fh->fcb->fcb.written_bytes - start_pos;//fh->fcb->fcb.size_in_bytes;
    if (SimpleFS_read(fh, text, size) == -1) 
    {
        printf("rd_error: cannot read from file\n");
        free(text);
        SimpleFS_close(fh);
        return;
    }
	
	//text[size - 1] = '\0';
    printf("%s \n",text);

    free(text);
    SimpleFS_close(fh);
}

int main(int argc, char** argv[2])
{
    signal(SIGINT, FS_abortion);

    fs = (SimpleFS*) malloc(sizeof(SimpleFS));
    if(fs == NULL)
    {
        printf("shell_error: malloc on SimpleFS, returning -1\n");
        return -1;
    }

    cur = FS_setup(filename, &disk, fs);
    if(cur == NULL)
    {
        printf("shell_error: cannot setup the filesystem, returning -1\n");
        free(fs);
        return -1;
    }
    FS_status();

    printf("\n-------------------------------[SIMPLE_FS]-------------------------------\n");
    printf("\n\nWELCOME!\t;)\n");
    
    printf("Type 'help' for command list\n");

    char cmd[256];
    ////////////////////////////
    ////////////////////////////

    while(1)
    {
        printf("%s$ ", cur->dcb->fcb.name);

        fgets(cmd, 256, stdin);

        //printf("[TEST] your cmd...: %s\n", cmd);

        char* argv[2] = {NULL};
        argv[0] = strtok(cmd, " ");
        argv[1] = strtok(NULL, "\n");

        int argc = (argv != NULL) ? 2 : 1;

        if(argv[1] == NULL) argv[0][strlen(argv[0]) - 1] = 0;

        //if(argc == 1) printf("[TEST] your cmd parsed...: %s\n", argv[0]);
        //if(argc == 2) printf("[TEST] your cmd parsed...: %s %s\n", argv[0], argv[1]);

        if (strcmp(argv[0], "mkdir") == 0) FS_mkdir(argc, argv); 
        else if (strcmp(argv[0], "ls") == 0 || strcmp(argv[0], "l") == 0) FS_ls(); 
        else if (strcmp(argv[0], "cd") == 0) FS_cd(argc, argv); 
        else if (strcmp(argv[0], "wr") == 0)
        {
            if(argv[1] == NULL)//(argc != 2)
            {
                printf("wr_error: you must use 'write <file_name>' \n");
                continue;
            } 
            FS_wr(argc, argv, 0); 
        }
        else if (strcmp(argv[0], "rd") == 0) 
        {
            if(argv[1] == NULL)//(argc != 2)
            {
                printf("rd_error: you must use 'write <file_name>' \n");
                continue;
            } 
            FS_rd(argc, argv, 0);
        } 
        else if (strcmp(argv[0], "write") == 0) 
        {
            if(argv[1] == NULL)//(argc != 2)
            {
                printf("write_error: you must use 'write <file_name>' \n");
                continue;
            } 
            
            int start = 0;
            printf("\n>Insert starting position\t");
            scanf("%d", &start);
            getchar();
            FS_wr(argc, argv, start); 
            printf("\n");
        }
        else if (strcmp(argv[0], "read") == 0) 
        {
            if(argv[1] == NULL)//(argc != 2)
            {
                printf("read_error: you must use 'read <file_name>' \n");
                continue;
            } 
            
            int start = 0;
            printf("\n>Insert starting position\t");
            scanf("%d", &start);
            getchar();
            FS_rd(argc, argv, start); 
            printf("\n");
        }
        else if (strcmp(argv[0], "rm") == 0) FS_rm(argc, argv); 
        else if (strcmp(argv[0], "touch") == 0) FS_touch(argc, argv); 
        else if (strcmp(argv[0], "status") == 0) FS_status(); 
        else if (strcmp(argv[0], "help") == 0) FS_help(); 
        else if (strcmp(argv[0], "quit") == 0) FS_abortion(SIGINT); 
        else printf("\nInvalid cmd!\n");
    }

    return 0;
}