#ifndef VERSIONS_H
#define VERSIONS_H

#define DESC_LENGTH 50
#define VERSION_COUNT 7

#include "slist.h"

//using *** to indicate COW relevant operations in debug prints

typedef struct versionhead {
    int latest_num; //the latest version in this filesystem
} versionhead;

typedef struct versionent {
    int version_num; //this version number
    int root_num; //the inum of the root of this version
    char desc[DESC_LENGTH]; //the description of this version
} versionent;

void* get_versions();
void set_current_root(int inum, const char* desc);
int get_current_root();
slist* versions_list();
int new_version(const char* path, const char* desc);
void print_versions();
int versions_rollback(int rb);
void* get_garbage_bitmap();
//flow of write mknod truncate type operations
//copy /
//do a for loop that parses through string to find next directory
    //the first thing it finds should be "/"
    //allocate an inode for the new one and then copy that entire thing 
    //(if path="/") then set_current_root(with new inum, description of action)
//wish list
//first described method is a copy_directory method
//second described method is an update_directory method
//once update_directory is done, any action that needs to be done can be performed because new item will be found


#endif