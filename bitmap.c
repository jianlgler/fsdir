#include <stdio.h>
#include <stdlib.h>

#include "bitmap.h"

#define BYTE_DIM 8

// converts a block index to an index in the array,
// and a char that indicates the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num) //num == posizione di blocco in memoria
{
    BitMapEntryKey entry_k;
    entry_k.entry_num = num / BYTE_DIM; //indice entry 
    entry_k.bit_num = num % BYTE_DIM; //spiazzamento (resto divisione)
    return entry_k;
}

// converts a bit to a linear index
int BitMap_indexToBlock(int entry, uint8_t bit_num)
{
    if(entry < 0)
    {
        printf("[PARAM ERROR], %d\n", entry);
        return -1;
    }
    if(bit_num < 0)
    {
        printf("[PARAM ERROR], %d\n", bit_num);
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
        printf("[PARAM ERROR], %d\n", status);
        return -1;
    }
    if(start > bmap -> num_bits || start < 0 )
    {
        printf("[PARAM ERROR], %d\n", start);
        return -1;
    }
    if(bmap == NULL)
    {
        printf("[PARAM ERROR], NullBitMap_exception\n");
        return -1;
    }

    int pos;
    for(int i = start; i < bmap->num_bits; i++)
    {
        BitMapEntryKey entry = BitMap_blockToIndex(i);
        int index = entry.entry_num;
        //X AND 1 ---> if x == 1 torna 1 else torna 0
        if((bmap -> entries[index] >> entry.bit_num & 1) == status) return i; 
        //se non va and 1 provare and 0x01 
    }

    printf("No block with status %d.\n", status);
    printf("No block with status %d..\n", status);
    printf("No block with status %d...\n", status);
    printf("Return value = -1"); 
    return -1;

}

// sets the bit at index pos in bmap to status
// torna -1 in caso di errore, 0 altrimenti
int BitMap_set(BitMap* bmap, int pos, int status) 
{
    if(status != 0 && status != 1)
    {
        printf("[PARAM ERROR], %d\n", status);
        return -1;
    }
    if(pos > bmap -> num_bits || pos < 0 )
    {
        printf("[PARAM ERROR], %d\n", pos);
        return -1;
    }
    if(bmap == NULL)
    {
        printf("[PARAM ERROR], NullBitMap_exception\n");
        return -1;
    } 

    BitMapEntryKey entry = BitMap_blockToIndex(pos);

    uint8_t mask = 1 >> (7 - entry.bit_num); //not sure da provare

    //X AND 1 ---> if x == 1 torna 1 else torna 0

    if(status)
    {
        
        bmap->entries[entry.entry_num] |= mask;
    }
    else 
    {
        bmap->entries[entry.entry_num] &= ~(mask);
    }

    return 0;
}


void BitMap_print(BitMap* bm)
{
    for(int i = 0; i < bm -> num_bits; i++)
    {
        BitMapEntryKey entry_k = BitMap_blockToIndex(i);
        //prendo il blocco, shifto al bit e confronto con 1, == andava comunque bene (credo...)
        int status = bm->entries[entry_k.entry_num] >> entry_k.bit_num & 1;

        printf("Entry_num = %d\tBit_num = %d\nStatus = %d\n---------------------------",
            entry_k.entry_num, entry_k.bit_num, 
            status);
    }
}