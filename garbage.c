
#include "garbage.h"
#include "bitmap.h"
#include "versions.h"
#include "inode.h"
#include "directory.h"
#include "util.h"
#include <assert.h>

void
collect_version_used(int root_num, void* gbm)
{
    //traverse full tree of filesystem, marking all used inodes
    bitmap_put(gbm, root_num, 1);
    inode* node = get_inode(root_num);
    void* directory = pages_get_page(node->ptrs[0]);
    void* ibm = get_inode_bitmap();
    for(int ii = 0; ii < 4096; ii+=sizeof(dirent)) {
        dirent* current = (dirent*)(directory + ii);
        //printf("current dirent %d and %s", current->inum, current->name);
        if(current->inum != 0 && bitmap_get(ibm, current->inum) == 1) {
            bitmap_put(gbm, current->inum, 1);
            if(get_inode(current->inum)->mode == 16877) {
                collect_version_used(current->inum, gbm);
            }
        }
    }
    printf(" + collect_version_used(root_num: %d)", root_num);
}

void
mark_and_sweep(void* gbm)
{
    //simply call collect_version_used for every valid versionent
    versionent* versions = (versionent*) get_versions();
    versionhead* v_head = (versionhead*)pages_get_page(1);
    int version_amount = v_head->latest_num;
    for(int ii = 0; ii < VERSION_COUNT; ii++) {
        versionent current = versions[ii];
        int inum = current.root_num;
        if(current.root_num != 0) {
            printf("mark and sweep THE ROOT NUM IS %d\n", inum);
            collect_version_used(inum, gbm);
            //bitmap_print(gbm, INODE_COUNT);
            puts("\nnext");
        }
    }
}

int
free_garbage(void* gbm)
{
    //loop through 0 .. INODE_COUNT and free every inode num where bitmap entry is 0 in garbage bitmap
    puts("Debugging: This is the bitmap of garbage marked bitmaps\n");
    bitmap_print(gbm, INODE_COUNT);
    puts("\nGarbage bitmap print done");
    void* ibm = get_inode_bitmap();
    for(int ii = 0; ii < INODE_COUNT; ii++) {
        if(bitmap_get(gbm, ii) == 0) {
            if(bitmap_get(ibm, ii) == 1) {
                printf("\nGarbage Collector freeing inum: %d", ii);
                free_inode(ii);
            }
        }
    }
    return 0;
}

void
garbage_collect()
{
    //reset bitmap for this use
    void* gbm = get_garbage_bitmap();
    bitmap_reset(gbm, INODE_COUNT);
    //call mark and sweep
    mark_and_sweep(gbm);
    //call free marked inodes
    int rv = free_garbage(gbm);
    assert(rv == 0);
    //reset bitmap for next use doing twice just to be super safe
    bitmap_reset(gbm, INODE_COUNT);
}