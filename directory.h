#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <bsd/string.h>

#include "slist.h"
#include "pages.h"
#include "inode.h"
#define DIR_NAME 48

typedef struct dirent {
    char name[DIR_NAME];
    int  inum;
    char _reserved[12];
} dirent;

slist* directories(const char* path);
dirent* directory_get(inode* parent, int inum);
void directory_init();
int directory_lookup(inode* dd, const char* name);
int tree_lookup(const char* path);
int directory_put(inode* parent, const char* name, int inum);
int directory_delete(const char* name);
slist* directory_list(const char* path);
//void print_directory(); just for debugging i assume, i have print statement everywhere anyway
int directory_parent(const char* path);
int directory_check_path(const char* path);
int copy_directory(inode* old_node, inode* new_node);
void replace_dir_ent(inode* parent_node, const char* target_path, int new_dir_inum, int old_dir_inum);

#endif

