#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h> 

#include "shell.h"

SimpleFS* fs;
DiskDriver* disk;
DirectoryHandle* cur;

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

void FS_abortion(int signal)
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