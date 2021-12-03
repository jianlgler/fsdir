#include "bitmap.c" 
#include "disk_driver.c"
#include "simplefs.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h> 

const char* filename = "./test1.txt";

int print_dir(DirectoryHandle* cur)
{	
	int* flag = (int*)malloc((cur->dcb->num_entries) * sizeof(int));

	for (int i = 0; i < cur->dcb->num_entries; i++) flag[i] = -1;
	
	if(flag == NULL)
  {
		printf("read_dir error, malloc on flag\n");
		return -1;
	}
	
	char** names = (char**)malloc((cur->dcb->num_entries) * sizeof(char*));
	if(names == NULL)
  {
		printf("read_dir error, malloc on names\n");
		return -1;
	}
	
	//for (int i = 0; i < (cur->dcb->num_entries); i++) names[i] = (char*)malloc(128*sizeof(char)); non serve + xk usiamo strdup in readdir
	//printf("content of %s:\t(Block in disk: %d)\n\n",cur->dcb->fcb.name, cur->dcb->fcb.block_in_disk);

  printf("Number of entries: %d\n\n", cur->dcb->num_entries);
	//printf("\nSUPPPP\n");
  if(SimpleFS_readDir(names, cur, flag) == -1)
  {
    printf("Error on SimpleFS_readDir");
    //for (int i = 0; i < cur->dcb->num_entries; i++) free(names[i]); 
    free(flag);
    free(names);
    return -1;
  }

  for (int i = 0; i < cur->dcb->num_entries; i++) 
  {
    if(flag[i] == 1) printf("%s\n", names[i]);
		
		//IsFile
		else if (flag[i] == 0)
    {
			if(names[i] != NULL) printf("%s\n",names[i]);
		}
	} 
  for (int i = 0; i < cur->dcb->num_entries; i++) free(names[i]); 
  free(names);
  free(flag);
  return 0;
}


int main(int agc, char** argv) 
{ 
 /*
  printf("\n------------------BITMAP TEST---------------------\n");

  BitMap* bm = (BitMap*) malloc(sizeof(BitMap));
  char entry[8] = { 0 };
  bm->num_bits = 64; bm->entries = entry;
  printf("Seq:\t");
  for(int i = 0; i < 8*BYTE_DIM; i++)
  {
    int res = BitMap_get(bm, i, 0);
    printf("%d ", res);
  } printf("\nDone\n");

  if(BitMap_set(bm, 9, 1) == -1)
  {
    printf("Error\n");
    return -1;
  }
  if(BitMap_set(bm, 15, 1) == -1)
  {
    printf("Error\n");
    return -1;
  }
  if(BitMap_set(bm, 30, 1) == -1)
  {
    printf("Error\n");
    return -1;
  }
  if(BitMap_set(bm, 21, 1) == -1)
  {
    printf("Error\n");
    return -1;
  }
  if(BitMap_set(bm, 60, 1) == -1)
  {
    printf("Error\n");
    return -1;
  }
 
   printf("New_Seq:\t");
  for(int i = 0; i < 8*BYTE_DIM; i++)
  {
    int res = BitMap_get(bm, i, 0);
    printf("%d ", res);
  } printf("\nDone\n");

  printf("BitMap 1:--------------------------------------------\n");

  BitMap* bm1 = (BitMap*) malloc(sizeof(BitMap));
  char bit1[] = "00000001";
  bm1->entries = bit1;
  bm1->num_bits = 64;
  BitMap_print(bm1);

  printf("------------------------------------------\n");


  printf("BitMap 2:--------------------------------------------\n");

  BitMap* bm2 = (BitMap*) malloc(sizeof(BitMap));
  char bit2[] = "10011001";
  bm2->entries = bit2; 
  bm2->num_bits = 64;
  BitMap_print(bm2);

  printf("BlocktoIndex-----------------------------------------\n");

  for(int i = 0; i < 64; i+=4)
  {
    BitMapEntryKey k = BitMap_blockToIndex(i);
    printf("Block: %d,\tto Index\tEntry num = %d (expected = %d), offset (bitnum) = %d (expected = %d)\n", i, k.entry_num, i/8, k.bit_num, i%8);
  }
    
  printf("IndextoBlock-----------------------------------------\n");

  for(int i = 0; i < 64; i+=4)
  {
    int entry = i/8; int offset = i%8;

    int block = BitMap_indexToBlock(entry, offset);

    printf("Entry num: %d,\tOffset: %d\tto Block\tBlock = %d (expected = %d)\n", i/8, i%8, block, entry*8 + offset);
  }
    
  printf("GET--------------------------------------------------\n");
    //0 is 00110000
    //1 is 00110001
  printf("[BITMAP_1]\n\n"); //00000001;
  int start = 0; int status = 0;
  printf("bit %d\tstarting from %d\t\t- expected %d\tresult is %d\n", 
                            status, start, 0,  BitMap_get(bm1,  start, status));
  start = 0;  status = 1;
  printf("bit %d\tstarting from %d\t\t- expected %d\tresult is %d\n", 
                            status, start, 4,  BitMap_get(bm1,  start, status));
  start = 17;  status = 0;
  printf("bit %d\tstarting from %d\t- expected %d\tresult is %d\n", 
                            status, start, 17,  BitMap_get(bm1,  start, status));                        
  start = 17;  status = 1;
  printf("bit %d\tstarting from %d\t- expected %d\tresult is %d\n", 
                            status, start, 20,  BitMap_get(bm1,  start, status));

  start = 60;  status = 0;
  printf("bit %d\tstarting from %d\t- expected %d\tresult is %d\n", 
                            status, start, 62,  BitMap_get(bm1,  start, status));

  start = 47;  status = 1;
  printf("bit %d\tstarting from %d\t- expected %d\tresult is %d\n", 
                            status, start, 52,  BitMap_get(bm1,  start, status));

  start = 62;  status = 1;
  printf("bit %d\tstarting from %d\t- expected %d\tresult is %d\n", 
                            status, start, -1,  BitMap_get(bm1,  start, status));

  printf("\nSET-------------------------------------------------\n");

  int pos, ret;
    
  pos = 0; status = 1; ret = BitMap_set(bm1, pos, status); 
  printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
  printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);

  pos = 0; status = 0; ret = BitMap_set(bm1, pos, status); 
  printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
  printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);

  pos = 10; status = 1; ret = BitMap_set(bm1, pos, status); 
  printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
  printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);

  pos = 25; status = 1; ret = BitMap_set(bm1, pos, status); 
  printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
  printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);

  pos = 25; status = 0; ret = BitMap_set(bm1, pos, status); 
  printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
  printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);

    //Decommentare per vedere gestione errori
    /*pos = 66; status = 1; ret = BitMap_set(bm1, pos, status); 
    printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
    printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);

    pos = 42; status = 5; ret = BitMap_set(bm1, pos, status); 
    printf("Setting position %d\tto %d\t", pos, status); !ret ? printf("[SUCCES]\n") : printf("[FAIL]\n");
    printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);
    *//*
  printf("\nBITMAP OK!\n");
  free (bm); free(bm1); free(bm2);
  */
  printf("\n------------------DISK_DRIVER TEST------------------------\n");
  
  if(remove(filename) == -1) printf("Error while trying to remove file\n");
    
  int ret;

  DiskDriver disk;
 
  DiskDriver_init(&disk, filename, 8);

  printf("DiskDriver inizializzato-------------------\n\n");
    
  DiskDriver_flush(&disk);
  printf("\nFlushTest OK\n\n");

  DiskDriver_print(&disk);

  printf("filename:\t%s\n", filename);

  char src[BLOCK_SIZE ]; char dest[BLOCK_SIZE ];

  printf("\n-----------------------------[OPERAZIONI SU DISKDRIVER VUOTO, ASPETTARSI ERRORI]-----------------------\n");
  printf("-------------READING\n");
  for(int i = 0; i < disk.header->num_blocks; i++)
  {
    if(DiskDriver_readBlock(&disk, dest, i) != -1)
    {
      printf("Expected error, got unknown...\n");
      return -1;
    }
    else
    {
      printf("...as expected\n");
    }
  }

  printf("-------------FREEING\n");
  for(int i = 0; i < disk.header->num_blocks; i++)
  {
    if(DiskDriver_freeBlock(&disk, i) != 0)
    {
      printf("Expected error, got unknown...iteration %d\n", i);
      return -1;
    }
    else
    {
      printf("...as expected\n");
    }
  }

  printf("\n----------------------------- [GETFREEBLOCK TEST__CROSSCHECK] -----------------------\n");
  BitMap* bm_d = (BitMap*) malloc(sizeof(BitMap));
  bm_d->entries = disk.bitmap_data; bm_d->num_bits = disk.header->bitmap_blocks;
  for(int i = 0; i < disk.header->num_blocks; i++)
  {
    int ret = DiskDriver_getFreeBlock(&disk, i);
    int ret_1 = BitMap_get(bm_d, i, 0);
    if(ret == -1)
    {
      printf("getFreeBlock error\n"); return -1;
    }
    printf("(DiskDriver_getFreeBlock:%d\tBitMap_get:%d\texpected: %d)\n", ret, ret_1, i);
  }
  free(bm_d);
  //-----------------------------------------------------------------------------------------------------------------------------------
  printf("\n\nDONE...ENDING\n");

  for(int i = 0; i < BLOCK_SIZE; i++) src[i] = 'D';
  src[BLOCK_SIZE - 1] = '\0';

  printf("\n----------------------------- [DD WR] -----------------------\n");

//SCRITTURA BLOCCHI
for(int i = 0; i < disk.header->num_blocks; i++)
{
 int ret = DiskDriver_writeBlock(&disk, src, i);

  if(ret == -1)
  {
    printf("disk_wr error\n");
    return -1;
  }
  printf("Wrote %d bytes in block %d\n\n", ret, i);
  printf("\n\n------[DISK STATUS]-------\n");
  DiskDriver_print(&disk);
}
  printf("\n----------------------------- [DD RD] -----------------------\n");


for(int i = 0; i < disk.header->num_blocks; i++)
{
  ret = DiskDriver_readBlock(&disk, dest, i);

  if(ret == -1)
  {
    printf("disk_wr error\n");
    return -1;
  }

  printf("\nContent_disk_1:\n%s", dest);
  
}
  printf("  \n-------------FREEING\n");
  for(int i = 0; i < disk.header->num_blocks; i++)
  {
    if(DiskDriver_freeBlock(&disk, i) == -1)
    {
      printf("Free error...iteration %d\n", i);
      return -1;
    }
    
  }

  printf("\n----------------------------- [FREE BLOCK PRINT] -----------------------\n");

  for(int i = 0; i < disk.header->num_blocks; i++)
  {
    int ret = DiskDriver_getFreeBlock(&disk, i);
    if(ret == -1)
    {
      printf("getFreeBlock error\n"); return -1;
    }
    printf("(DiskDriver_getFreeBlock:%d\t\texpected: %d)\n", ret, i);
  }




  printf("\n-----------------------------[DISKDRIVER_TEST DONE]-----------------------\n");
  
  if(remove(filename) == -1)
  {
    printf("Error while trying to remove file\n");
    return -1;
  }
  
  printf("\n-----------------------------[SIMPLEFS_TEST]------------------------------\n");


  DiskDriver_init(&disk, filename, 100); //DiskDriver_init(&disk, filename, 7);
  DiskDriver_flush(&disk);
  SimpleFS* fs = (SimpleFS*) malloc(sizeof(SimpleFS));
  if(fs == NULL)
  {
    printf("test_error: malloc on sfs");
    return -1;
  }

  printf("Created diskdriver with 100 blocks\n");
  printf("\n");
  DiskDriver_print(&disk); printf("\n");

  DirectoryHandle* cur = SimpleFS_init(fs, &disk);
  if(cur == NULL)
  {
    printf("test_error: sfs not initialized!!\n");

   return -1;
  } 


  printf("Current directory: %s\tBlock in disk: %d\n", cur->dcb->fcb.name, cur->dcb->fcb.block_in_disk);

  /*printf("DOUBT CHEK\n\n");
  printf("%d\t%d\t%d\n", disk.header->first_free_block, DiskDriver_getFreeBlock(&disk, 0), DiskDriver_getFreeBlock(&disk, disk.header->first_free_block));*/

  printf("------------------------[CREATING FILES]----------------------------------\n");

    //creazione polkadot, kusama, monero, moonriver, cardano

  FileHandle* polkadot = SimpleFS_createFile(cur, "polkadot.txt");
  if(polkadot == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }

  FileHandle* cardano = SimpleFS_createFile(cur, "cardano.txt");
  if(cardano == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }

  FileHandle* kusama = SimpleFS_createFile(cur, "kusama.txt");
  if(kusama == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }

  FileHandle* monero = SimpleFS_createFile(cur, "monero.txt");
  if(monero == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }

  FileHandle* moonriver = SimpleFS_createFile(cur, "moonriver.txt");
  if(moonriver == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }

  /*FileHandle* joe = SimpleFS_createFile(cur, "joe.txt");
  if(joe == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }*/

  printf("Five files created: polkadot.txt, kusama.txt, monero.txt, moonriver.txt, cardano.txt\n");
  //printf("Six files created: polkadot.txt, kusama.txt, monero.txt, moonriver.txt, cardano.txt, joe.txt\n");
  
  if(print_dir(cur) == -1)
  {
    printf("Errore print dir, RIP");
    free(fs); 
    return -1;
  }
  
  if(SimpleFS_close(polkadot) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  if(SimpleFS_close(cardano) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  if(SimpleFS_close(kusama) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

   if(SimpleFS_close(monero) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  if(SimpleFS_close(moonriver) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  printf("\nCreating another file after previously closing the other five\n");
  
  FileHandle* joe = SimpleFS_createFile(cur, "joe.txt");
  if(joe == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }
  
  if(print_dir(cur) == -1)
  {
    printf("Errore print dir, RIP");
    free(fs); 
    return -1;
  }

  if(SimpleFS_close(joe) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  printf("\n\n------------[DISK STATUS]-----------\n\n");
  DiskDriver_print(fs->disk);

  printf("Trying to fill the whole disk\n");

  int i = disk.header->first_free_block;
  while(disk.header->first_free_block != -1)
  {
    
    char name[10]; 
    sprintf(name,"%d_fh.txt", i);
    //printf("\n");
    FileHandle* fh = SimpleFS_createFile(cur, name);
    if(fh == NULL)
    {
      printf("Errore in creazione file, RIP");
      free(fs); 
      return -1;
    }
    if(SimpleFS_close(fh) == -1)
    {
      printf("Error: could not close file\n");
	    free(fs); 
	    return -1;
    }
    i += 1;
  }

  DiskDriver_print(fs->disk);
  if(print_dir(cur) == -1)
  {
    printf("Errore print dir, RIP");
    free(fs); 
    return -1;
  }

  printf("Trying now to create another file, should be full\n");

  FileHandle* eth = SimpleFS_createFile(cur, "eth.txt");
  if(eth != NULL)
  {
    printf("Errore in creazione file, expected error, RIP\n");
    free(fs); 
    return -1;
  } printf("...as expected\n");
  

  
  

  printf("\n-------------------------[OPENING TEST]-------------------------------\n");

    
  FileHandle* dot_fh = SimpleFS_openFile(cur, "polkadot.txt");
  if(dot_fh == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }
  printf("File %s opened, closing now...\n", dot_fh->fcb->fcb.name);
  SimpleFS_close(dot_fh);

  FileHandle* ksm_fh = SimpleFS_openFile(cur, "kusama.txt");
  if(ksm_fh == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }
  printf("\nFile %s opened, closing now...\n", ksm_fh->fcb->fcb.name);
  SimpleFS_close(ksm_fh);

  FileHandle* ada_fh = SimpleFS_openFile(cur, "cardano.txt");
  if(ada_fh == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }
  printf("\nFile %s opened, closing now...\n", ada_fh->fcb->fcb.name);
  SimpleFS_close(ada_fh);

  FileHandle* mo_fh = SimpleFS_openFile(cur, "monero.txt");
  if(mo_fh == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }
  printf("\nFile %s opened, closing now...\n", mo_fh->fcb->fcb.name);
  SimpleFS_close(mo_fh);

  FileHandle* movr = SimpleFS_openFile(cur, "moonriver.txt");
  if(movr == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }
  printf("\nFile %s opened, closing now...\n", movr->fcb->fcb.name);
  SimpleFS_close(movr);
  //apro file casualmente
  i = 7;
  while(cur->dcb->file_blocks[i] != -1 && i < 99)
  {
    if( i%11 == 0)
    {
      char name[10]; 
      sprintf(name,"%d_fh.txt", i);
      printf("\n");
      FileHandle* fh = SimpleFS_openFile(cur, name);
      if(fh == NULL)
      {
        printf("Errore in creazione file, RIP");
        free(fs); 
        return -1;
      }
      printf("\nFile %s opened, closing now...\n", fh->fcb->fcb.name);
      SimpleFS_close(fh);
    }
    i += 1;
  }

  printf("------------------------[WRRD TEST]---------------------\n");
  printf("Per scrivere in un file bisogna aprirlo\n");
  int bw = 0, br = 0;
  char* tw = "Fender Stratocaster made in Japan, 1989";
  char tr[1024] = " ";

  polkadot = SimpleFS_openFile(cur, "polkadot.txt");
  if(polkadot == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }

  //printf("OK\n");
  bw = SimpleFS_write(polkadot, tw, strlen(tw));
  if(bw != strlen(tw))
  {
    printf("Error: write eror\n");
    free(fs);
    return -1;
  }
  printf("Wrote %d bytes in %s\n", bw, polkadot->fcb->fcb.name);

  printf("Reading now!\n");

  br = SimpleFS_read(polkadot, tr, bw);
  if(br != bw)
  {
    printf("Error: read eror\n");
    free(fs); 
    return -1;
  }
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, polkadot->fcb->fcb.name, tr);
  memset(tr, 0, strlen(tr));
  

  cardano = SimpleFS_openFile(cur, "cardano.txt");
  if(cardano == NULL)
  {
    printf("Errore in apertura file, RIP\n");
    free(fs); 
    return -1;
  }

  tw = "Amor che move l'sole e l'altre stelle";
  //printf("OK\n");
  bw = SimpleFS_write(cardano, tw, strlen(tw));
  if(bw != strlen(tw))
  {
    printf("Error: write eror\n");
    free(fs);
    return -1;
  }
  printf("Wrote %d bytes in %s\n", bw, cardano->fcb->fcb.name);

  printf("Reading now!\n");

  br = SimpleFS_read(cardano, tr, bw);
  if(br != bw)
  {
    printf("Error: read eror\n");
    free(fs); 
    return -1;
  }
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, cardano->fcb->fcb.name, tr);
  memset(tr, 0, strlen(tr));
  SimpleFS_close(cardano);

  printf("------------------------[SEEK TEST]---------------------\n");
  tw = "Fender Stratocaster made in Japan, 1989";
  int offset = 10;
  if(SimpleFS_seek(polkadot, offset) == -1)
  {
    printf("SEEK_ERROR\n");
    SimpleFS_close(polkadot);
    return -1;
  }
  br = SimpleFS_read(polkadot, tr, strlen(tw) - offset);
  if(br != strlen(tw) - offset)
  {
    printf("Error: read eror\n");
    free(fs); 
    return -1;
  }
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, polkadot->fcb->fcb.name, tr);
  memset(tr, 0, strlen(tr));

  if(SimpleFS_seek(polkadot, 0) == -1)
  {
    printf("SEEK_ERROR\n");
    SimpleFS_close(polkadot);
    return -1;
  }

  printf("\nRiporto il cursore di polkadot a 0\n");

  SimpleFS_close(polkadot);


  printf("\n--------------------[REMOVE TEST]---------------------------------\n\n");

  if(SimpleFS_remove(cur, "joe.txt") == -1)
  {
    printf("RMV error\n");
    return -1;
  }

  DiskDriver_print(fs->disk);
  if(print_dir(cur) == -1)
  {
    printf("Errore print dir, RIP");
    free(fs); 
    return -1;
  }
  i = 7;
  while(cur->dcb->file_blocks[i] != -1 && i < 99)
  {    
    char name[10]; 
    sprintf(name,"%d_fh.txt", i);
    //printf("\n");
    if(SimpleFS_remove(cur, name) == -1)
    {
      printf("RMV error\n");
      return -1;
    }
    i += 1;
  }
  printf("\n\n");
  DiskDriver_print(fs->disk);
  if(print_dir(cur) == -1)
  {
    printf("Errore print dir, RIP");
    free(fs); 
    return -1;
  }

  printf("Recreating joe \n");

  joe = SimpleFS_createFile(cur, "joe.txt");
  if(joe == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); 
    return -1;
  }
  
  if(print_dir(cur) == -1)
  {
    printf("Errore print dir, RIP");
    free(fs); 
    return -1;
  }

  if(SimpleFS_close(joe) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }
  
  //printf("COPYING ROOT\n");
  //DirectoryHandle* root = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));
  //memcpy(root, cur, sizeof(DirectoryHandle));


  printf("\n--------------------[MAKING NEW DIRECTORY]---------------------------------\n\n");
  

  if(SimpleFS_mkDir(cur, "Jianl") == -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); 
    return -1;
  } 

  printf("Contenuto directory:\n\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); 
	  return -1;
	}
  
  printf("Changing dir, going in Jianl\n");

  
  
  if(SimpleFS_changeDir(cur, "Jianl") == -1)
  {
    printf("Error while changing dir\n");
    if(cur != NULL) SimpleFS_free_dir(cur);

    free(fs);

    return -1;
  }
  
  printf("We in %s now\n---------------------[populating]------------------\n", cur->dcb->fcb.name);

  
  FileHandle* a = SimpleFS_createFile(cur, "a.txt");
  if(a == NULL)
  {
     printf("Error: could not create file a.\n");
	  free(fs); 
	  return -1;
  }

  if(SimpleFS_close(a) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }
  
  //printf("%s\n", cur->dcb->fcb.name);

  FileHandle* b = SimpleFS_createFile(cur, "a.txt");
  if(b != NULL)
  {
     printf("Error: could not create file a.\n");
	  free(fs); 
	  return -1;
  } printf("\tas expected\n");

  FileHandle* c = SimpleFS_createFile(cur, "c.txt");
  if(c == NULL)
  {
     printf("Error: could not create file a.\n");
	  free(fs); 
	  return -1;
  }
  if(SimpleFS_close(c) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  /*
    creare directory Jianl_2 dentro Jianl e riempire anche quella, poi stampare tutto, poi rimuovere Jianl, rimuovendo anche Jianl_2
    stampare '/'
  */

  if(SimpleFS_mkDir(cur, "Jianl_2") == -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); 
    return -1;
  } 
  printf("\n\nSiamo in jianl, creata cartella jianl_2\n\n-----------------------\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); 
	  return -1;
	}

  if(SimpleFS_changeDir(cur, "Jianl_2") == -1) 
  {
    printf("Error while changing dir\n");
    if(cur != NULL) SimpleFS_free_dir(cur);

    free(fs);

    return -1;
  }
  printf("WE IN JIANL_2\n");

  //----------------------------------------------------------------------------------------
  FileHandle* uno = SimpleFS_createFile(cur, "uno.txt");
  if(uno == NULL)
  {
     printf("Error: could not create file a.\n");
	  free(fs); 
	  return -1;
  }
  if(SimpleFS_close(uno) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }
  FileHandle* due = SimpleFS_createFile(cur, "due.txt");
  if(due == NULL)
  {
     printf("Error: could not create file a.\n");
	  free(fs); 
	  return -1;
  }
  if(SimpleFS_close(due) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }
  FileHandle* tre = SimpleFS_createFile(cur, "tre.txt");
  if(tre == NULL)
  {
     printf("Error: could not create file a.\n");
	  free(fs); 
	  return -1;
  }
  if(SimpleFS_close(tre) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); 
	  return -1;
  }

  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); 
	  return -1;
	}
  
  printf("JIANL 2 -> cd ..\n");
  if(SimpleFS_changeDir(cur, "..") == -1) 
  {
    printf("Error while changing dir\n");
    if(cur != NULL) SimpleFS_free_dir(cur);

    free(fs);

    return -1;
  }
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); 
	  return -1;
	}
  printf("JIANL  -> cd ..\n");
  if(SimpleFS_changeDir(cur, "..") == -1) 
  {
    printf("Error while changing dir\n");
    if(cur != NULL) SimpleFS_free_dir(cur);

    free(fs);

    return -1;
  }

  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); 
	  return -1;
	}

  DiskDriver_print(&disk);
  printf("\n\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n\n");
 
  if(SimpleFS_remove(cur, "Jianl") == -1)
  {
    printf("RMV error\n");
    return -1;
  }

  printf("RIMOSSO JIANL, con sideeffect\n");

  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); 
	  return -1;
	}

  DiskDriver_print(&disk);

  printf("Closed everything, exiting...\n");

  //free(cur);
  if(cur != NULL) SimpleFS_free_dir(cur);
  
  free(fs);

  return 0;
  
}
