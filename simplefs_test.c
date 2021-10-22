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

FileBlock* init_block(char value){
	int i;
	FileBlock* fb = (FileBlock*)malloc(sizeof(FileBlock));
	if(fb == NULL)
  {
		fprintf(stderr,"Error: malloc create_file_block\n");
		return NULL;
	}

	char data_block[BLOCK_SIZE - sizeof(BlockHeader)];

	for(i = 0; i < (BLOCK_SIZE - sizeof(BlockHeader)); i++) data_block[i] = value;

	data_block[BLOCK_SIZE - sizeof(BlockHeader) -1] = '\0';
	strcpy(fb->data, data_block); //(dest, src)
	return fb;
}


int main(int agc, char** argv) {
 
    printf("\n------------------BITMAP TEST----------------------\n");

  	printf("BitMap 1:--------------------------------------------\n");

    BitMap* bm1 = (BitMap*) malloc(sizeof(BitMap));
    char bit1[] = "00000001";
    bm1->entries = bit1;
    bm1->num_bits = 64;
    BitMap_print(bm1);

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
      printf("Block: %d,\tto Index\tEntry num = %d (expected = %d), offset (bitnum) = %d (expected = %d)\n",
                                i, k.entry_num, i/8, k.bit_num, i%8);
    }
    
    printf("IndextoBlock-----------------------------------------\n");

    for(int i = 0; i < 64; i+=4)
    {
      int entry = i/8; int offset = i%8;

      int block = BitMap_indexToBlock(entry, offset);

      printf("Entry num: %d,\tOffset: %d\tto Block\tBlock = %d (expected = %d)\n",
                              i/8, i%8, block, entry*8 + offset);
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
    printf("Expected %d\tresult is %d\n", BitMap_get(bm1,  pos, status), pos);*/

    printf("\nBITMAP OK!\n");
    printf("\n------------------DISK_DRIVER TEST----------------------\n");

    if(remove(filename) == -1)
    {
      printf("Error while trying to remove file\n");
    }

    DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver)); //stampa no free block perchè al running precedente del test sono ancora allocati
    DiskDriver_init(disk, filename, 8);

    printf("DiskDriver inizializzato-------------------\n\n");
    
    DiskDriver_flush(disk);
    printf("\nFlushTest OK\n\n");

    DiskDriver_print(disk);

    printf("filename:\t%s\n", filename);

    printf("\nCreating now 8 blocks:\n");
    
    FileBlock* fb_0 = init_block('0');
    if(fb_0 == NULL)
    {
      printf("Impossibile inizializzare blocco 0 RIP\n");
      return -1;
    }
    printf("Blocco 0 inizializzato!\n");

    FileBlock* fb_1 = init_block('1');
    if(fb_1 == NULL)
    {
      printf("Impossibile inizializzare blocco 1 RIP\n");
      return -1;
    }
    printf("Blocco 1 inizializzato!\n");

    FileBlock* fb_2 = init_block('2');
    if(fb_2 == NULL)
    {
      printf("Impossibile inizializzare blocco 2 RIP\n");
      return -1;
    }
    printf("Blocco 2 inizializzato!\n");

    FileBlock* fb_3 = init_block('3');
    if(fb_3 == NULL)
    {
      printf("Impossibile inizializzare blocco 3 RIP\n");
      return -1;
    }
    printf("Blocco 3 inizializzato!\n");

    FileBlock* fb_4 = init_block('4');
    if(fb_4 == NULL)
    {
      printf("Impossibile inizializzare blocco 4 RIP\n");
      return -1;
    }
    printf("Blocco 4 inizializzato!\n");

    FileBlock* fb_5 = init_block('5');
    if(fb_5 == NULL)
    {
      printf("Impossibile inizializzare blocco 5 RIP\n");
      return -1;
    }
    printf("Blocco 5 inizializzato!\n");

    FileBlock* fb_6 = init_block('6');
    if(fb_6 == NULL)
    {
      printf("Impossibile inizializzare blocco 6 RIP\n");
      return -1;
    }
    printf("Blocco 6 inizializzato!\n");

    FileBlock* fb_7 = init_block('7');
    if(fb_7 == NULL)
    {
      printf("Impossibile inizializzare blocco 7 RIP\n");
      return -1;
    }
    printf("Blocco 7 inizializzato!\n");

    /*--------------------------------------------------  Free test     --------------------------------------------------------------------------*/

    printf("\nLiberando blocchi scritti su disco nelle precedenti esecuzioni del test\n");
    for(int i = 0; i < 8; i++) DiskDriver_freeBlock(disk, i);

    /*--------------------------------------------------  Get Free BLock test     ----------------------------------------------------------------*/
    printf("\nGetFreeBlock test\n");

    for(int i = 0; i < 8; i++)
    {
      printf("%d", DiskDriver_getFreeBlock(disk, i));
    }
    printf("[CORRECT = 01234567]\n");
    /*--------------------------------------------------  Block writing --------------------------------------------------------------------------*/

    printf("\n\nEight blocks creates, writing them on disk!\n");

    //------------------------------------------------------------0
    printf("\nWriting block 0 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_0, 0) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 0 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------1
    printf("\nWriting block 1 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_1, 1) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 1 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------2
    printf("\nWriting block 2 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_2, 2) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 2 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------3
    printf("\nWriting block 3 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_3, 3) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 3 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------4
    printf("\nWriting block 4 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_4, 4) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 4 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------5
    printf("\nWriting block 5 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_5, 5) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 5 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------6
    printf("\nWriting block 6 to disk... \n");
	  if(DiskDriver_writeBlock(disk, fb_6, 6) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 6 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    DiskDriver_print(disk);

        //------------------------------------------------------------7
    printf("\nWriting block 7 to disk... \n");
    if(DiskDriver_writeBlock(disk, fb_7, 7) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
    {
		  printf("Error: could not write block 7 to disk\n");
		  return -1;
	  }
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  } 
    printf("\n");
    DiskDriver_print(disk);
    printf("\nDisk is FULL now!\n");
    printf("\n\n------------------------------------[STATUS]--------------------------------\n");

    DiskDriver_print(disk);

    printf("\nTrying to write a block now that disk is full, should return ERR_VALUE\n");
    FileBlock* fb_8 = init_block('8');
    if(fb_8 == NULL)
    {
      printf("Impossibile inizializzare blocco 8 RIP\n");
      return -1;
    }

    if(DiskDriver_writeBlock(disk, fb_8, 8) == -1) printf("Error: could not write block 8 to disk....as expected\n");
	  
	  if(DiskDriver_flush(disk) == -1)
    {
		  printf("Error: flush\n");
		  return -1;
	  }
    printf("\n");
    //--
}

