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
DiskDriver* disk;
DirectoryHandle* cur;

const char* filename = "./my_file.txt";

DirectoryHandle* FS_setup(const char* filename, DiskDriver* disk, SimpleFS* simple_fs)
{
    DiskDriver_init(filename, disk, 1024);
    DiskDriver_flush(disk);

    DirectoryHandle* dh = SimpleFS_init(simple_fs,disk);
    if(dh == NULL)
    {
        SimpleFS_format(simple_fs);
		dh = SimpleFS_init(simple_fs,disk);
    }
    return dh;
}

void FS_abortion(int dummy)
{
    printf("\nClosing"); printf("."); printf("."); printf(".\n");

    if(cur != NULL) SimpleFS_free_dir(cur);
    if(disk != NULL) free(disk);
    if(fs != NULL) free(fs);

    exit(EXIT_SUCCESS); 
}

void FS_status()
{
    DiskDriver_print(disk);
}

void FS_help()
{
    printf("\n");
    printf("Use mkdir <directory_name> to create new directory\n");
    printf("Use wr <file_name> to write inside a file\n");
    printf("Use rd <file_name> to read from a it\n");
    printf("Use touch <file_name> to create an empty file\n");
    printf("Use ls to view a list of the files and folders in current directory \n");
    printf("Use cd <directory_name> to move into the selected directory\n");
    printf("Use rm <file or directory name> to remove a file or a directory with its content\n");
    printf("Use status to show disk informations\n");
    printf("Use quit or CTRL + C to exit the program\n\n");
    printf("Use help to show a list of the avaible commands (it repeats current messages)\n");
}

void FS_rm(int argc, char* argv[2])
{
    if(argc != 2)
    {
        printf("rm_error: you must use 'rm <file or directory name>' \n");
        return;
    }

    if(SimpleFS_remove(cur, argv[1]) == -1) printf("rm_error: cannot remove %s \n", argv[1]);
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
        SimpleFS_close(fh);
    } 
}

void FS_cd(int argc, char* argv[2])
{
    if(argc != 2)
    {
        printf("cd_error: you must use 'cd <directory_name>' \n");
        return;
    } 
    if (SimpleFS_changeDir(cur, argv[1]) == -1)
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
	for (int i = 0; i < cur->dcb->num_entries; i++) 
    {
		flags[i] = -1;
	}

	char** names = (char**)malloc((cur->dcb->num_entries) * sizeof(char*)); //array di stringhe
	if(names == NULL)
    {
		fprintf(stderr,"ls_error in readDirectory: malloc of contents.\n");
        free(flags);
        free(names);
		return;
	}
	for (int i = 0; i < (cur->dcb->num_entries); i++) 
    {
		names[i] = (char*)malloc(128*sizeof(char)); //inizializzo ogni stringa
	}
	
    if(SimpleFS_readDir(names, cur, flags) == -1) //lettura e inizializzazione array di interesse
    {
    printf("ls_error: could not use readDir\n");
    free(flags);
    free(names);
    return;
    }

    for (int i = 0; i < cur->dcb->num_entries; i++) 
    {
        if(flags[i] == 1) //Directory
        { //testo directory in azzurro
            if(i%3 == 0) printf("\033[1;34m%s\033[0m\n", names[i]); 
            else printf("\033[1;34m%s\033[0m\t", names[i]);
        }
        if(flags[i] == 0) //File, white
        {
            if(i%3 == 0) printf("%s\n", names[i]);
            else printf("%s\t", names[i]);
        }

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

    if(argc != 2)
    {
        printf("wr_error: you must use 'wr <file_name>' \n");
        return;
    } 

    FileHandle* fh = SimpleFS_openFile(cur, argv[1]);
    if(fh == NULL)
    {
        printf("wr_error: cannot open %s \n", argv[1]);
        return;
    }

    if(SimpleFS_seek(fh, start_pos) == -1)
    {
        printf("wr_error: cannot jump to starting position \n", argv[1]);
        SimpleFS_close(fh);
        return;
    }

    printf("\n>");
    char* t = (char*) malloc(sizeof(BLOCK_SIZE));
    fgets(t, BLOCK_SIZE, stdin);

    if (SimpleFS_write(fh, t, strlen(t)) == -1) 
    {
        printf("wr_error: error while writeing\n");
        free(t);
        SimpleFS_close(fh);
        return;
    }

    free(t);
    SimpleFS_close(fh);
}

void FS_rd(int argc, char* argv[2])
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

    char* text = (char*) malloc(BLOCK_SIZE*sizeof(char));
     if(text == NULL)
     {
		printf("rd_error: cannot allocate memory for text\n");
		SimpleFS_close(fh);
		return;
	}
    
    int size = fh->fcb->fcb.size_in_bytes;
    if (SimpleFS_read(fh, text, size) == -1) 
    {
        printf("rd_error: cannot read from file\n");
        free(text);
        SimpleFS_close(fh);
        return;
    }
	
	text[size - 1] = '\0';
    printf("%s \n",text);

    free(text);
    SimpleFS_close(fh);
}

int main(int argc, int argv[2])
{
    signal(SIGINT, FS_abortion);

    fs = (SimpleFS*) malloc(sizeof(SimpleFS));
    if(fs == NULL)
    {
        printf("shell_error: malloc on SimpleFS, returning -1\n");
        return -1;
    }

    disk = (DiskDriver*) malloc(sizeof(DiskDriver*));
    if(disk == NULL)
    {
        printf("shell_error: malloc on DiskDriver, returning -1\n");
        free(fs);
        return -1;
    }


    cur = FS_setup(filename, disk, fs);
    if(cur == NULL)
    {
        printf("shell_error: cannot setup the filesystem, returning -1\n");
        free(disk);
        free(fs);
        return -1;
    }

    char welcome[] = "Welcome";

    for(int i = 0; i < strlen(welcome); i++)
    {
        printf("%c", welcome[i]);
    } 
    sleep(1000);
    printf("\t;)\n");

    printf("Type 'help' for command list\n");

    char cmd[256];

    while(1)
    {
        printf("%s$", cur->dcb->fcb.name);

        fgets(cmd, 256, stdin);

        char* argv[2] = NULL;
        argv[0] = strtok(cmd, " ");
        argv[1] = strtok(NULL, "\n");
    }
}