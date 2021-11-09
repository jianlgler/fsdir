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

FileBlock* init_block(char value)
{
	FileBlock* fb = (FileBlock*)malloc(sizeof(FileBlock));
	if(fb == NULL)
  {
		printf("init_error: malloc on fb\n");
		return NULL;
	}

	char data_block[BLOCK_SIZE - sizeof(BlockHeader)];

	for(int i = 0; i < (BLOCK_SIZE - sizeof(BlockHeader)); i++) data_block[i] = value;

	data_block[BLOCK_SIZE - sizeof(BlockHeader) -1] = '\0';
	strcpy(fb->data, data_block); //(dest, src).
	return fb;
}

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
	
	for (int i = 0; i < (cur->dcb->num_entries); i++) names[i] = (char*)malloc(128*sizeof(char));
	
	//printf("\nSUPPPP\n");
  if(SimpleFS_readDir(names, cur, flag) == -1)
  {
    printf("Error on SimpleFS_readDir");
    for (int i = 0; i < cur->dcb->num_entries; i++) free(names[i]); 
    free(flag);
    free(names);
    return -1;
  }
	
	printf("content of %s:\n\n",cur->dcb->fcb.name);

  printf("Number of entries: %d\n\n", cur->dcb->num_entries);

  for (int i = 0; i < cur->dcb->num_entries; i++) 
  {
    if(flag[i] == 1) printf("%s\n", names[i]);
		
		//IsFile
		else if (flag[i] == 0)
    {
			if(names[i] != NULL) printf("%s\n",names[i]);
		}
    //printf("\tblock_num:%d\n", cur->dcb->file_blocks[i]);
	} 
  //for (int i = 0; i < cur->dcb->num_entries; i++) free(names[i]); 
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
    
  printf("\nBITMAP OK!\n");
  free (bm); free(bm1); free(bm2);
  */
  printf("\n------------------DISK_DRIVER TEST----------------------\n");
  
  if(remove(filename) == -1) printf("Error while trying to remove file\n");
    
  printf("\nok\n");
  DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver)); //stampa no free block perchè al running precedente del test sono ancora allocati
  printf("\nok1\n");
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

    printf("--------------------------------------------------  Free test     --------------------------------------------------------------------------\n");

  printf("\nLiberando blocchi scritti su disco nelle precedenti esecuzioni del test\n");
  for(int i = 0; i < 8; i++) DiskDriver_freeBlock(disk, i);

    printf("--------------------------------------------------  Get Free BLock test     ----------------------------------------------------------------\n");
  printf("\nGetFreeBlock test\n");

  for(int i = 0; i < 8; i++) printf("%d", DiskDriver_getFreeBlock(disk, i));
    
  printf("[CORRECT = 01234567]\n");
    printf("--------------------------------------------------  Block writing --------------------------------------------------------------------------\n");

  printf("\n\nEight blocks creates, writing them on disk!\n");

    //------------------------------------------------------------0
  printf("\nWriting block 0 to disk... \n");
	if(DiskDriver_writeBlock(disk, fb_0, 0) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
  {
	  printf("Error: could not write block 0 to disk\n");
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

  printf("\n");
  DiskDriver_print(disk);

        //------------------------------------------------------------2
  printf("\nWriting block 2 to disk... \n");
	if(DiskDriver_writeBlock(disk, fb_2, 2) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
  {
		printf("Error: could not write block 2 to disk\n");
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

  printf("\n");
  DiskDriver_print(disk);

        //------------------------------------------------------------4
  printf("\nWriting block 4 to disk... \n");
	if(DiskDriver_writeBlock(disk, fb_4, 4) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
  {
		printf("Error: could not write block 4 to disk\n");
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

  printf("\n");
  DiskDriver_print(disk);

        //------------------------------------------------------------6
  printf("\nWriting block 6 to disk... \n");
	if(DiskDriver_writeBlock(disk, fb_6, 6) == -1) //nella writeblock c'è la BM_get, che stampera "NO BLOCK WITH STATUS 1, returning -1", perchè giustamente tutti i blocchi sono liberi
  {
		printf("Error: could not write block 6 to disk\n");
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

  printf("\n");

  printf("-------------------------------------------Reading test----------------------------\n");

  FileBlock* aux = (FileBlock*) malloc(sizeof(FileBlock));

  for(int i = 0; i < 8; i++)
  {
    printf("Reading block %d...\n", i);
    if(DiskDriver_readBlock(disk, aux, i) == -1)
    {
      printf("test_error: diskdriver readBlock");
      return -1;
    }
    printf("Read successfully:)\nDATA:\t%s\n", aux->data);
  }

  printf("Liberando blocchi now\n");

  for(int i = 0; i < 8; i++) 
  {
    printf("Freeing block %d...\n", i);
    if(DiskDriver_freeBlock(disk, i) == -1)
    {
      printf("test_error: diskdriver readBlock");
      return -1;
    }
  }

  printf("\n\nDONE...ENDING\n");

  free(disk);
  free(fb_0); free(fb_1); free(fb_2); free(fb_3); free(fb_4); free(fb_5); free(fb_6); free(fb_7); free(fb_8);
  free(aux);
  
  printf("\n-----------------------------[DISKDRIVER_TEST DONE]-----------------------\n");
  
  if(remove(filename) == -1)
  {
    printf("Error while trying to remove file\n");
    return -1;
  }
  
  printf("\n-----------------------------[SIMPLEFS_TEST]------------------------------\n");

  disk = (DiskDriver*) malloc(sizeof(DiskDriver)); //stampa no free block perchè al running precedente del test sono ancora allocati
  DiskDriver_init(disk, filename, 100);
  DiskDriver_flush(disk);
  SimpleFS* fs = (SimpleFS*) malloc(sizeof(SimpleFS));
  if(fs == NULL)
  {
    printf("test_error: malloc on sfs");
    return -1;
  }

  printf("Created diskdriver with 100 blocks\n");
  printf("\n");
  DiskDriver_print(disk); printf("\n");

  DirectoryHandle* cur = SimpleFS_init(fs, disk);
  if(cur == NULL)
  {
    printf("test_error: sfs not initialized!!\n");

    printf("Need format!\n");
    printf("formatting...\n\n");
    SimpleFS_format(fs);

    cur = SimpleFS_init(fs, disk);
  } 
  else
  {
    printf("Success, recovering structures\n");
    cur = SimpleFS_init(fs, disk);
  }
    
  printf("Current directory: %s\n", cur->dcb->fcb.name);

  printf("------------------------[CREATING FILES]----------------------------------\n");

    //creazione polkadot, kusama, monero, moonriver, cardano

  FileHandle* polkadot = SimpleFS_createFile(cur, "polkadot.txt");
  if(polkadot == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }

  FileHandle* cardano = SimpleFS_createFile(cur, "cardano.txt");
  if(cardano == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }

  FileHandle* kusama = SimpleFS_createFile(cur, "kusama.txt");
  if(kusama == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }

  FileHandle* monero = SimpleFS_createFile(cur, "monero.txt");
  if(monero == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }

  FileHandle* moonriver = SimpleFS_createFile(cur, "moonriver.txt");
  if(moonriver == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }

  printf("Five files created: polkadot.txt, kusama.txt, monero.txt, moonriver.txt, cardano.txt\n");
  printf("\n--------------------[READING DIRECTORY]---------------------------------\n\n");
	
	  //Leggo il contenuto della directory /
	if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}

  printf("Provo a creare un file già esistente, kusama, dovrebbe dare errore\n");
  FileHandle* kusama_dup = SimpleFS_createFile(cur, "kusama.txt");

  printf("\n-------------------------[OPENING TEST]-------------------------------\n");

    
  FileHandle* dot_fh = SimpleFS_openFile(cur, "polkadot.txt");
  if(dot_fh == NULL)
  {
    printf("Errore in apertura file, RIP");
    free(fs); free(disk);
    return -1;
  }
  printf("File %s opened, closing now...\n", dot_fh->fcb->fcb.name);
  SimpleFS_close(dot_fh);

  FileHandle* ksm_fh = SimpleFS_openFile(cur, "kusama.txt");
  if(ksm_fh == NULL)
  {
    printf("Errore in apertura file, RIP");
    free(fs); free(disk);
    return -1;
  }
  printf("File %s opened, closing now...\n", ksm_fh->fcb->fcb.name);
  SimpleFS_close(ksm_fh);

  FileHandle* ada_fh = SimpleFS_openFile(cur, "cardano.txt");
  if(ada_fh == NULL)
  {
    printf("Errore in apertura file, RIP");
    free(fs); free(disk);
    return -1;
  }

  printf("File %s opened, closing now...\n", ada_fh->fcb->fcb.name);
  SimpleFS_close(ada_fh);

  printf("\n--------------------[MAKING NEW DIRECTORY]---------------------------------\n\n");

  if(SimpleFS_mkDir(cur, "Jianl") == -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); free(disk);
    return -1;
  } printf("Directory named 'Jianl' created :)\nCreating now directory with same name, error expected\n");

  if(SimpleFS_mkDir(cur, "Jianl") != -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); free(disk);
    return -1;
  } printf("\n...as expected\n");

  printf("Contenuto directory:\n\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}

  printf("\n--------------------[WR/RD TEST]---------------------------------\n\n");
  
  printf("Scrivo su tre file, polkadot, cardano e kusama\n\n");

  printf("---------------------------DOT WRRD------------------------------\n\n");

  int bw = 0, br = 0;
  char* tw = "Fender Stratocaster made in Japan, 1989";
  char* tr = (char*) malloc(sizeof(char));

  bw = SimpleFS_write(polkadot, tw, strlen(tw));
  if(bw != strlen(tw))
  {
    printf("Error: write eror\n");
    free(fs); free(disk); free(tr);
    return -1;
  }
  printf("Wrote %d bytes in %s\n", bw, polkadot->fcb->fcb.name);

  printf("Reading now!\n");

  br = SimpleFS_read(polkadot, tr, bw);
  if(br != bw)
  {
    printf("Error: read eror\n");
    free(fs); free(disk); free(tr);
    return -1;
  }
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, polkadot->fcb->fcb.name, tr);
  memset(tr, 0, strlen(tr));

  //---------------------------------------
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk); free(tr);
	  return -1;
	}

  
  printf("---------------------------ADA WRRD------------------------------\n\n");
  tw = "Gymnopedie - Erik Satie";

  bw = SimpleFS_write(cardano, tw, strlen(tw));
  if(bw != strlen(tw))
  {
    printf("Error: write eror\n");
    free(fs); free(disk); free(tr);
    return -1;
  }
  printf("Wrote %d bytes in %s\n", bw, cardano->fcb->fcb.name);

  printf("Reading now!\n");

  br = SimpleFS_read(cardano, tr, bw);
  if(br != bw)
  {
    printf("Error: read eror\n");
    free(fs); free(disk); free(tr);
    return -1; 
  } 
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, cardano->fcb->fcb.name, tr);
  
  memset(tr, 0, strlen(tr));
  //----------------------------------------
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk); free(tr);
	  return -1;
	}
  printf("---------------------------KSM WRRD------------------------------\n\n");
  tw = "BlowUp - Michelangelo Antonioni\tMasculine Feminine - Jean-Luc Godard\tDillinger is dead - Marco Ferreri";
  
  bw = SimpleFS_write(kusama, tw, strlen(tw));
  if(bw != strlen(tw))
  {
    printf("Error: write eror\n");
    free(fs); free(disk); free(tr);
    return -1;
  }
  printf("Wrote %d bytes in %s\n", bw, kusama->fcb->fcb.name);

  printf("Reading now!\n");

  br = SimpleFS_read(kusama, tr, bw);
  if(br != bw)
  {
    printf("Error: read eror\n");
    free(fs); free(disk); free(tr);
    return -1; 
  }
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, kusama->fcb->fcb.name, tr);

  memset(tr, 0, strlen(tr));

  //---------------------------------------
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk); free(tr);
	  return -1;
	}
 

  printf("\n\n----------------------------[SEEK TEST]----------------------------\n\n");

  tw = "Fender Stratocaster made in Japan, 1989";
  bw = strlen(tw);

  if(SimpleFS_seek(polkadot, 33) == -1)
  {
    printf("Error: seek error\n");
    free(fs); free(disk);  free(tr);
    return -1; 
  }

  printf("Seek done, reading now!\n");

  br = SimpleFS_read(polkadot, tr, bw - 33);
  if(br != bw - 33)
  {
    printf("Error: read eror\n");
    free(fs); free(disk);  free(tr);
    return -1;
  }
  printf("%d byte letti in %s\nContenuto: '%s'\n", br, polkadot->fcb->fcb.name, tr);

  if(SimpleFS_seek(polkadot, bw + 5) != -1)
  {
    printf("Seek_error: should return error!"); 
    free(fs); free(disk);  free(tr);
    return -1; 
  }

  printf("Position too high, as expected\n");
 
  SimpleFS_seek(polkadot, 0); printf("Cursore riportato a posizione zero in polkadot\n");


  printf("\n\n----------------------------[CHANGE DIR TEST]----------------------------\n\n");
  free(tr);
  if(SimpleFS_changeDir(cur, "Jianl") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");

  if(print_dir(cur) != -1) //expected error cuz empty
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk); 
	  return -1;
	} printf("\n...As expected\n");

  printf("Going back to '/'\n");
  if(SimpleFS_changeDir(cur, "..") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");
  if(print_dir(cur) == -1) //expected error cuz empty
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}

  printf("\n\nRemoving 2 files\n");

  if(SimpleFS_remove(cur, "monero.txt")  == -1)
  {
    printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(monero) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("Monero removed\n");
  //free(monero);

  if(SimpleFS_remove(cur, "moonriver.txt")  == -1)
  {
    printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(moonriver) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("moonriver removed\n");
  //free(moonriver);

  printf("OK\n");
  if(print_dir(cur) == -1) 
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}

  printf("\n-----------------------------[POPULATING]----------------------------------------\n\n");

  printf("Going to 'Jianl'\n");
  if(SimpleFS_changeDir(cur, "Jianl") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n\n");
  printf("Current directory:\t%s\n", cur->dcb->fcb.name);

  FileHandle* moonsama = SimpleFS_createFile(cur, "moonsama.txt");
  if(moonsama == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  printf("Moonsama created\n");
 //
  
  FileHandle* moonbeam = SimpleFS_createFile(cur, "moonbeam.txt");
  if(moonbeam == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  printf("Moonbeam created\n");
  
  FileHandle* solamander = SimpleFS_createFile(cur, "solamander.txt");
  if(solamander == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  printf("Solamander created\n");
  if(SimpleFS_remove(cur, "solamander.txt") == -1)
  {
    printf("Error: could not remove 'Movies'\n");
	  free(fs); free(disk);
	  return -1;
  }
  FileHandle* cryptopunk = SimpleFS_createFile(cur, "cryptopunk.txt");
  if(cryptopunk == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  printf("Cryptopunk created\n");
  
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
    
  printf("\n--------------------[MAKING NEW DIRECTORY]---------------------------------\n\n");

  if(SimpleFS_mkDir(cur, "Videogames") == -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); free(disk);
    return -1;
  } printf("Directory named 'Videogames' created :)\n");

  printf("Contenuto directory:\n\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}
  
  
  if(SimpleFS_mkDir(cur, "Music") == -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); free(disk);
    return -1;
  } printf("Directory named 'Music' created :)\n");

  printf("Contenuto directory:\n\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}

  if(SimpleFS_mkDir(cur, "Movies") == -1)
  {
    printf("Errore mkdir, RIP");
    free(fs); free(disk);
    return -1;
  } printf("Directory named 'Movies' created :)\n");

  printf("Contenuto directory:\n\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
	}
//-------------------------------------------------------------------------------------------------------------------------
  printf("\nMoving into Videogames to populate it\n\n");
  if(SimpleFS_changeDir(cur, "Videogames") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");

  printf("Current directory:\t%s\n", cur->dcb->fcb.name);

  FileHandle* FFX = SimpleFS_createFile(cur, "FinalFantasy_X.txt");
  if(FFX == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  
  FileHandle* mtr2033 = SimpleFS_createFile(cur, "Metro_2033.txt");
  if(mtr2033 == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
 
  FileHandle* undertale = SimpleFS_createFile(cur, "Undertale.txt");
  if(undertale == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  //------------------------------------------------------------
  printf("\nGoing back to Jianl, then jumpin into Music to populate it\n\n");
  if(SimpleFS_changeDir(cur, "..") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  if(SimpleFS_changeDir(cur, "Music") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");

  printf("Current directory:\t%s\n", cur->dcb->fcb.name);

  FileHandle* blackmarket = SimpleFS_createFile(cur, "Black_Market - Weather_Report.txt");
  if(blackmarket == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  
  FileHandle* saw = SimpleFS_createFile(cur, "Selected_Ambient_Works_vol.1 - Aphex_Twin.txt");
  if(saw == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
 
  FileHandle* fo = SimpleFS_createFile(cur, "Freak_out - Frank_Zappa.txt");
  if(fo == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  //------------------------------------------------------------
  printf("\nGoing back to Jianl, then jumpin into Movies to populate it\n\n");
  if(SimpleFS_changeDir(cur, "..") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
 
  if(SimpleFS_changeDir(cur, "Movies") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");

  printf("Current directory:\t%s\n", cur->dcb->fcb.name);

  FileHandle* ran = SimpleFS_createFile(cur, "Ran - Akira_Kurosawa.txt");
  if(ran == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
  
  FileHandle* dec_v = SimpleFS_createFile(cur, "La_decima_vittima - Elio_Petri.txt");
  if(dec_v == NULL)
  {
    printf("Errore in creazione file, RIP");
    free(fs); free(disk);
    return -1;
  }
 
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  
   //------------------------------------------------------------
  printf("\nGoing back to Jianl, then removing Movies and all of its content, removing also some file\n\n");
  if(SimpleFS_changeDir(cur, "..") == -1)
  {
    printf("test: changedir error, cannot move to 'Jianl'\n");
    free(fs); free(disk); 
    return -1; 
  }

  printf("Current directory:\t%s\n", cur->dcb->fcb.name);
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  //------------------------------------------------------------
  if(SimpleFS_remove(cur, "Movies") == -1)
  {
    printf("Error: could not remove 'Movies'\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("\n\n'Movies' removed successfully\n");


  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_remove(cur, "moonsama.txt") == -1)
  {
    printf("Error: could not remove 'moonsama.txt'\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(moonsama) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("moonsama removed\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("\n---------------------------------[NAVIGATING THROUGH THE FS]-------------------------\n \n");
  printf("\nGoing to VIdeogames\n");
  if(SimpleFS_changeDir(cur, "Videogames") == -1)
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");
    if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("BAck to Jianl then to /\n");
  if(SimpleFS_changeDir(cur, "..") == -1)
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }

  if(SimpleFS_changeDir(cur, "Music") == -1)
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");
    if(print_dir(cur) == -1)//we in music
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  
  printf("BAck to Jianl then to /\n");
  if(SimpleFS_changeDir(cur, "..") == -1)//we in jianl
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");
    if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_changeDir(cur, "..") == -1)//we  /
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("OK\n");
    if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("\nTRying to bo back, expected error\n");
    if(SimpleFS_changeDir(cur, "..") != -1)//we in /
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  printf("As expected...\n\n");
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  printf("\n-------------------------------------------[CLOSING FILES]---------------------------------\n");
  if(SimpleFS_close(fo) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(saw) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(blackmarket) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(FFX) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(mtr2033) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(undertale) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(cryptopunk) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(SimpleFS_close(moonbeam) == -1)
  {
    printf("Error: could not close file\n");
	  free(fs); free(disk);
	  return -1;
  }
  
  printf("moving in jianl\n");
  if(SimpleFS_changeDir(cur, "Jianl") == -1)
  {
       printf("test: changedir error, cannot move to '/'\n");
    free(fs); free(disk); 
    return -1; 
  }
  if(print_dir(cur) == -1)
  {
	  printf("Error: could not read current dir.\n");
	  free(fs); free(disk);
	  return -1;
  }
  if(cur != NULL) SimpleFS_free_dir(cur);
  free(fs);
  free(disk);
  printf("Closed everything, exiting...\n"); return 0;
  
}
