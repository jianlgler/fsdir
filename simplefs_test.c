#include "bitmap.c" 
#include "disk_driver.c"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h> 

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

    DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver));
    DiskDriver_init(disk, "./test.txt", 42);
    BitMap bm;
    //bm.num_bits = disk->header->bitmap_blocks;
    //bm.entries = disk->bitmap_data;

    printf("DiskDriver inizializzato e bitmap creata-------------------\n\n");

    DiskDriver_print(disk);
}
