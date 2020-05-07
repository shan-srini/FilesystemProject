
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "versions.h"
#include "inode.h"
#include "util.h"
#include "directory.h"
#include "garbage.h"

void*
get_versions()
{
    //for now storing all version information in page 1
    void* base = (void*)(pages_get_page(1) + sizeof(versionhead));
    return base;
}

// to be used as a REUSABLE bitmap of size INODE_COUNT (256) for garbage collection
// keeping in versions (for organization purposes) because it uses page 1
void*
get_garbage_bitmap()
{
    int buffer = sizeof(versionent); //just in case something not working right
    void* base = get_versions() + (VERSION_COUNT * sizeof(versionent));
    return base;
}

// sets the current root directory inum
void
set_current_root(int inum, const char* desc)
{
    //sticking a header on there at beginning of page for simplicity
    versionhead* v_head = pages_get_page(1);
    versionent* versions = (versionent*) get_versions();
    //have to go backwards to shift correctly
    for(int ii = VERSION_COUNT-1; ii > 0; ii--) {
        versionent* new_pos = &(versions[ii]);
        versionent to_shift = versions[ii-1];
        new_pos->version_num = to_shift.version_num;
        new_pos->root_num = to_shift.root_num;
        strcpy(new_pos->desc, to_shift.desc);
    }
    versionent* insert = (versionent*) get_versions();
    insert->root_num = inum;
    insert->version_num = v_head->latest_num; 
    v_head->latest_num += 1;
    strcpy(insert->desc, desc);
    printf("*** set_current_root(inum: %d, desc: %s)\n", insert->root_num, &(insert->desc));
    garbage_collect();
}

//return inum of the current root node
int
get_current_root()
{
    versionent* versions = get_versions();
    return versions->root_num;
}

//slist of version entries for cowtool
slist*
versions_list()
{
    //print_versions();
    versionhead* v_head = pages_get_page(1);
    versionent* versions = (versionent*) get_versions();
    int start = min(VERSION_COUNT, v_head->latest_num)-1;
    slist* toReturn = 0;
    for(int ii = start; ii>=0; ii--) {
        char buf[100];
        sprintf(buf, "%d %s", versions[ii].version_num, versions[ii].desc);
        toReturn = s_cons(buf, toReturn);
    }
    return toReturn;
}

//this function will create a new version of the filesystem
//with appropriate dirents updated with new inum
int
new_version(const char* path, const char* desc)
{
    int new_root_inum = alloc_inode();
    char cur_path[DIR_NAME];
    cur_path[0] = '/';
    inode* old_dir = get_inode(get_current_root());
    inode* new_dir = get_inode(new_root_inum);
    copy_directory(old_dir, new_dir);
    new_dir->mode = 040755;
    new_dir->size = old_dir->size;

    inode* parent_node = new_dir;
    for(int ii = 1; ii<DIR_NAME; ii++) {
        cur_path[ii] = path[ii];
        cur_path[ii+1] = '\0';
        int old_dir_inum = directory_check_path(cur_path);
        if(old_dir_inum != -ENOENT) {
            old_dir = get_inode(old_dir_inum);
            int new_dir_inum = alloc_inode();
            new_dir = get_inode(new_dir_inum);
            //check directory
            if(old_dir->mode == 16877) {
                copy_directory(old_dir, new_dir);
                replace_dir_ent(parent_node, cur_path, new_dir_inum, old_dir_inum);
            } else {
                //this is a file that is being changed
                printf("found file to copy. %s\n", cur_path);
                copy_inode(old_dir, new_dir);
                replace_dir_ent(parent_node, cur_path, new_dir_inum, old_dir_inum);
                //I think this is okay because the file has to be at the end of the path...
                new_dir->mode = old_dir->mode;
                new_dir->size = old_dir->size;
                new_dir->refs - old_dir->refs;
                new_dir->time_access = old_dir->time_access;
                new_dir->time_created = old_dir->time_created;
                new_dir->time_modified = old_dir->time_modified;
                break;
            }
            //replace_dir_ent(parent_node, cur_path, new_dir_inum);
            new_dir->mode = old_dir->mode;
            new_dir->size = old_dir->size;
            new_dir->refs - old_dir->refs;
            new_dir->time_access = old_dir->time_access;
            new_dir->time_created = old_dir->time_created;
            new_dir->time_modified = old_dir->time_modified;
            parent_node = new_dir;
        }
    }

    set_current_root(new_root_inum, desc);
    return 0;
}

void
print_versions()
{
    versionent* versions = get_versions();
    for(int ii = 0; ii<VERSION_COUNT; ii++) {
        printf("version num: %d, rootnum: %d, desc: %s\n", versions[ii].version_num, versions[ii].root_num, versions[ii].desc);
    }
}

int
versions_rollback(int rb)
{
    void* versions = get_versions();
    versionent* first = get_versions();
    for(int ii = 0; ii<VERSION_COUNT*sizeof(versionent); ii+=sizeof(versionent)) {
        versionent* current = versions + ii;
        if(current->version_num == rb) {
            versionent temp;
            temp.root_num = first->root_num;
            strcpy(temp.desc, first->desc);
            temp.version_num = first->version_num;
            first->root_num = current->root_num;
            first->version_num = current->version_num;
            strcpy(first->desc, current->desc);
            current->root_num = temp.root_num;
            current->version_num = temp.version_num;
            strcpy(current->desc, temp.desc);
            printf("rolledback to version: %d\n", ((versionent*)get_versions())->version_num);
            return 0;
        }
    }
    return -1;
}