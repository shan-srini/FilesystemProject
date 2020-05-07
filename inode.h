#ifndef INODE_H
#define INODE_H

#include "pages.h"
#include <sys/types.h>
#define NUM_PTRS 2
#define INODE_COUNT 256

typedef struct inode {
    mode_t mode; // permission & type; zero for unused
    int size; // bytes
    int ptrs[NUM_PTRS]; //direct ptrs
    int iptr; //indirect ptr
    int refs; //ref count
    time_t time_created;
    time_t time_access;
    time_t time_modified;
} inode;

void print_inode(inode* node);
inode* get_inode(int inum);
int alloc_inode();
void free_inode(int inum);
int inode_get_pnum(inode* node, int fpn);
int grow_inode(inode* node, int size);
int shrink_inode(inode* node, int size);
int inode_get_pnum(inode* node, int fpn);
void copy_inode(inode* source, inode* target);

#endif
