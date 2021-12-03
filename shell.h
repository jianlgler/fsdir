#include "simplefs.h"

DirectoryHandle* FS_setup(const char* filename, DiskDriver* disk, SimpleFS* simple_fs); //initialization
void FS_abortion(int dummy);
void FS_help();
void FS_status();
void FS_rm(int argc, char* argv[2]);
void FS_mkdir(int argc, char* argv[2]);
void FS_touch(int argc, char* argv[2]);
void FS_cd(int argc, char* argv[2]);
void FS_ls();
void FS_wr(int argc, char* argv[2], int start_pos);
void FS_rd(int argc, char* argv[2], int start_pos);