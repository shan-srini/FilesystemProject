
#include <stdint.h>
#include <time.h>

#include "pages.h"
#include "inode.h"
#include "util.h"
#include "bitmap.h"
#include "versions.h"


inode*
get_inode(int inum)
{
    uint8_t* base = (uint8_t*) pages_get_page(2);
    inode* nodes = (inode*)(base);
    return &(nodes[inum]);
}

int
alloc_inode()
{
    void* ibm = get_inode_bitmap();
    for (int ii = 1; ii < INODE_COUNT; ++ii) {
        if(bitmap_get(ibm, ii) == 0) {
            bitmap_put(ibm, ii, 1);
            inode* node = get_inode(ii);
            node->mode = 0;
            node->size = 0;
            node->ptrs[0] = alloc_page();
            node->iptr = 0;
            node->refs = 1;
            node->time_created = time(0);
            return ii;
        }
    }

    return -1;
}

void
free_inode(int inum)
{
    printf(" + free_inode(%d)\n", inum);
    inode* node = get_inode(inum);
    print_inode(node);
    node->mode = 0;
    void* ibm = get_inode_bitmap();
    if(bitmap_get(ibm, inum) != 1 || inum == 1) {
        printf("how did this inum get here...? %d", inum);
        return;
    }
    shrink_inode(node, 0);
    node->iptr = 0;
    node->mode = 0;
    node->ptrs[0] = 0;
    node->ptrs[1] = 0;
    node->refs = 0;
    node->size = 0;
    node->time_access = 0;
    node->time_created = 0;
    node->time_modified = 0;
    bitmap_put(ibm, inum, 0);
    puts("bitmap inode");
    //bitmap_print(ibm, 256);
}

void
print_inode(inode* node)
{
    if (node) {
        printf("node{mode: %04o, size: %d, directpage0: %d, directpage1: %d, iptr: %d}\n",
               node->mode, node->size, node->ptrs[0], node->ptrs[1], node->iptr);
    }
    else {
        printf("node{null}\n");
    }
}

int grow_inode(inode* node, int size)
{
    printf(" + grow_inode() to size: %d\n", size);
    int grow_size = size - node->size;
    int ii = node->size / 4096;
    while(grow_size > 0) {
        if(ii < NUM_PTRS) {
            if(node->ptrs[ii] == 0) {
                node->ptrs[ii] = alloc_page();
            }
            grow_size -= 4096;
        }
        else {
            if(!node->iptr) {
                //indirect pointer
                node->iptr = alloc_page();
            }
            //begin setting indirect pointer page array of ints to pointers at pages.
            int* iptr_page = (int*) pages_get_page(node->iptr);
            iptr_page[ii - NUM_PTRS] = alloc_page();
            grow_size -= 4096;
        }
        ii++;
    }
    node->size = size;
    return 0;
}

int shrink_inode(inode* node, int size) {
    printf("  + shrink_inode() to size %d\n", size);
    int pages_goal = size / 4096;
    int pages_have = node->size / 4096;
    int ii = pages_have;
    while(pages_goal <= ii) {
        if(ii < 2) {
            printf(" + shrink inode freeing direct");
            free_page(node->ptrs[ii]);
            node->ptrs[ii] = 0;
        }
        else {
            if(node->iptr > 1){
                    int* iptr_page = (int*) pages_get_page(node->iptr);
                    printf(" + shrink inode freeing indrect");
                    if(iptr_page[ii - NUM_PTRS] != 0) {
                    free_page(iptr_page[ii-NUM_PTRS]);
                    iptr_page[ii-NUM_PTRS] = 0;
                }
            }
            //I shouldn't hit this unless the page at iptr_page[0] doesn't exist.
            else {
                free_page(node->iptr);
                node->iptr = 0;
            }
        }
        ii--;
    }
    return 0;
}

//copy all the data in one inode to another inode
void
copy_inode(inode* source, inode* target)
{
    int ii = 0;
    int remaining_size = source->size;
    grow_inode(target, source->size-target->size);
    while(remaining_size >= 0) {
        if(ii<NUM_PTRS) {
            void* source_data = pages_get_page(source->ptrs[ii]);
            void* target_data = pages_get_page(target->ptrs[ii]);
            memcpy(target_data, source_data, min(4096, remaining_size));
        } else {
            int* source_page = (int*) pages_get_page(source->iptr);
            int* target_page = (int*) pages_get_page(target->iptr);
            void* source_data = pages_get_page(source_page[ii - NUM_PTRS]);
            void* target_data = pages_get_page(target_page[ii - NUM_PTRS]);
            memcpy(target_data, source_data, min(4096, remaining_size));
        }
        remaining_size -= 4096;
        ii++;
    }
    target->size = source->size;
    printf("Large copy done");
}

int inode_get_pnum(inode* node, int fpn);