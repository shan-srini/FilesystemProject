#ifndef GARBAGE_H
#define GARBAGE_H

#include "slist.h"

/* 
    wish list:
        need a bitmap stored in page 1, get it from versions.c
        need a bitmap reset function in bitmap.h/c
        need a method which traverses whole tree and marks used inodes that it sees
        need a method which frees every unmarked inode
*/

void collect_version_used(int root_num, void* gbm);
void mark_and_sweep(void* gbm);
int free_garbage(void* gbm);
void garbage_collect();

#endif