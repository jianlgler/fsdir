#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "bitmap.h"

#define BYTE_DIM 8

// converts a block index to an index in the array,
// and a char that indicates the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num) //num == posizione di blocco in memoria
{ 
    BitMapEntryKey entry_k = 
    {
        .entry_num = num / BYTE_DIM, //indice entry 
        .bit_num = num % BYTE_DIM //spiazzamento (resto divisione)
    };
    return entry_k;
}

// converts a bit to a linear index
int BitMap_indexToBlock(int entry, uint8_t bit_num) //op inversa
{
    if(entry < 0)
    {
        printf("[PARAM ERROR], entry, return value is -1");
        return -1;
    }
    if(bit_num < 0)
    {
        printf("[PARAM ERROR], bit_num, return value is -1");
        return -1;
    }
    
    return (entry * BYTE_DIM) + bit_num;
}

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start
int BitMap_get(BitMap* bmap, int start, int status)
{
    if(status != 0 && status != 1) 
    {
        printf("[PARAM ERROR], status, return value is -1");
        return -1;
    }
    
    if(start > bmap -> num_bits || start < 0 ) 
    {
        printf("[PARAM ERROR], start, return value is -1");
        return -1;
    }
    
    if(bmap == NULL) 
    {
        printf("[PARAM ERROR], bitmap, return value is -1");
        return -1;
    }
    
    //scorro il block index, ogni volta mi prendo l'entry key della quale faccio un bit-check
    for(int i = start; i < bmap->num_bits; i++)
    {
        BitMapEntryKey entry = BitMap_blockToIndex(i);
        int index = entry.entry_num;
        int bit = bmap -> entries[index] >> entry.bit_num & 0x01;
        //X AND 1 ---> if x == 1 torna 1 else torna 0
        if( bit == status) return i; 
        //se non va and 1 provare and 0x01 
    }

    /*printf("No block with status %d...", status);
    printf("Return value = -1\n"); */
    return -1;

}

// sets the bit at index pos in bmap to status
// torna -1 in caso di errore, 0 altrimenti
int BitMap_set(BitMap* bmap, int pos, int status) 
{   
    if(status != 0 && status != 1) 
    {
        printf("[PARAM ERROR], status, return value = -1\n");
        return -1;
    }
    
    if(pos > bmap -> num_bits || pos < 0 )
    {
        printf("[PARAM ERROR], pos, return value = -1\n");
        return -1;
    }
    if(bmap == NULL) 
    {
        printf("[PARAM ERROR], bmap, return value = -1\n");
        return -1;
    }
    
    BitMapEntryKey entry = BitMap_blockToIndex(pos);

    unsigned char mask = 1 << entry.bit_num; //not sure da provare
    
    //X AND 1 ---> if x == 1 torna 1 else torna 0
    
    //fin qui tutto ok
    if(status) bmap->entries[entry.entry_num] |= mask; //setting a bit
    else bmap->entries[entry.entry_num] &= ~(mask);    //clearing a bit

    return 0;
}


void BitMap_print(BitMap* bm)
{
    if(bm == NULL)
    {
        printf("[PARAM ERROR], bitmap is null, nothing to be done, returning");
        return;
    }

    for(int i = 0; i < bm -> num_bits; i++)
    {
        BitMapEntryKey entry_k = BitMap_blockToIndex(i);
        //prendo il blocco, shifto al bit e confronto con 1
        int status = bm->entries[entry_k.entry_num] >> entry_k.bit_num & 0x01;
        //printf("Value in BitMap entries = %c\t", bm->entries[entry_k.entry_num]);
        printf("Bit_num (abs) = %i\tEntry_num = %i\tOffset = %d\tStatus = %i\n",
            i, entry_k.entry_num, entry_k.bit_num, 
            status);
    }
}

int BitMap_isFree(BitMap* bmap, int block_num)
{
        if(block_num > bmap->num_bits || block_num < 0) return -1;

        BitMapEntryKey k = BitMap_blockToIndex(block_num);
        int idx = k.entry_num;
        int bit = bmap->entries[idx] >> k.bit_num & 0x01;

        if(bit == 0) return 1; else return 0;
}