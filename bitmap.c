
#include <stdio.h>
#include <stdint.h>
#include "bitmap.h"

// This is heavily referencing a stackOverflow implementation (I haven't had too much experience with bitwise operations)
//https://stackoverflow.com/questions/16947492/looking-for-a-bitmap-implementation-api-in-linux-c

int bitmap_get(void* bm, int ii)
{
    // check the bit at ii and return 0 or 1 accordingly
    int toReturn = ((uint8_t*)bm)[ii / 8] & (1 << (ii & 7)) ? 1 : 0;
    return toReturn;
}

void bitmap_put(void* bm, int ii, int vv)
{
    // if vv1 do or operator to get set ii to 1
    if(vv == 1) {
        ((uint8_t*)bm)[ii / 8] |= 1 << (ii & 7);
    }
    else {
        ((uint8_t*)bm)[ii / 8] &= ~(1 << (ii & 7));
    }
}

void bitmap_print(void* bm, int size)
{
    for(int ii = 0; ii < size; ++ii) {
        printf("%d", bitmap_get(bm, ii));
    }
}

//sets every bitmap entry to 0
void bitmap_reset(void* bm, int size)
{
    for(int ii = 0; ii < size; ++ii) {
        bitmap_put(bm, ii, 0);
    }
    //bitmap_print(bm, size);
}