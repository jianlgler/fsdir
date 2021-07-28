#include "bitmap.c" 
#include "disk_driver.c"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h> 

int main(int agc, char** argv) {
 
    printf("\n------------------BITMAP TEST---------------------\n");
    int num = 666;
    printf("BlocktoIndex-----------------------------------------\n");

    BitMapEntryKey entry = BitMap_blockToIndex(num);
    int e_bit = num%8; int e_num = num/8;
    printf("\nExpected bit = %d, actual bit = %d\n", e_bit, entry.bit_num);
    printf("\nExpected entry num = %d, actual entry num = %d\n", e_num, entry.entry_num);

    printf("IndextoBlock-----------------------------------------\n");

    printf("Expected block number = %d, actual block number = %d\n",
                                  num, BitMap_indexToBlock(e_num, e_bit)) ;

    printf("Get----------------------------------------\n");

    BitMap* bm = (BitMap*) malloc(sizeof(BitMap));
    bm->num_bits = 64;
    bm->entries = "11100100";

    int ret = BitMap_get(bm, 0, 0);
    printf("Searching %d with start: %d, expected = %d, real = %d", 0, 0, 12, ret);
    printf("\n");

    ret = BitMap_get(bm, 12, 0);
    printf("Searching %d with start: %d, expected = %d, real = %d", 0, 12, 12, ret);
    printf("\n");

    ret = BitMap_get(bm, 16, 0);
    printf("Searching %d with start: %d, expected = %d, real = %d", 0, 16, 16, ret);
    printf("\n");

    ret = BitMap_get(bm, 0, 1);
    printf("Searching %d with start: %d, expected = %d, real = %d", 1, 0, 0, ret);
    printf("\n");

    ret = BitMap_get(bm, 4, 1);
    printf("Searching %d with start: %d, expected = %d, real = %d", 1, 4, 4, ret);
    printf("\n");

    ret = BitMap_get(bm, 32, 1);
    printf("Searching %d with start: %d, expected = %d, real = %d", 1, 31, -1, ret);
    printf("\n");

    printf("Set-----------------------------------------\n");
  
}
