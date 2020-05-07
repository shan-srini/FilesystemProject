
#define _GNU_SOURCE
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "directory.h"
#include "pages.h"
#include "slist.h"
#include "util.h"
#include "inode.h"
#include "versions.h"

#define ENT_SIZE 16

void
directory_init()
{
    //inode for root directory
    //this will also grab a page for root in inode, no issue there
    int inum = alloc_inode();
    inode* rn = get_inode(inum);
    //set the current root inode number
    set_current_root(inum, "create root");
    printf(" + directory_init() -> inum: %d\n", inum);
    rn->size = 0;
    rn->mode = 040755;
}

//used in rename, return dirent* to change name
dirent*
directory_get(inode* parent, int inum) {
    int end = parent->size / sizeof(dirent);
    dirent* dir_page = pages_get_page(parent->ptrs[0]);
    for(int ii = 0; ii < end; ii++) {
        dirent current = dir_page[ii];
        if(current.inum == inum) {
            return &(dir_page[ii]);
        }
    }
}

// return inum of path in this given inode directory parent
int
directory_lookup(inode* dd, const char* name)
{
    //puts("Directory lookup");
    if (strcmp(name, "") == 0) {
        return 0;
    }
    dirent* parent_page = pages_get_page(dd->ptrs[0]);
    dirent current;

    //printf("contents of dir %s\n", name);

    for(int ii = 0; ii < 64; ii++) {
        current = parent_page[ii];
        if (strcmp(name, current.name) == 0) {
            //printf("FOUND IT: %s\n\n", current.name);
            return current.inum;
        }
    }

    return -ENOENT;
}

//puts a dir ent into an inode parent (directory) page
int
directory_put(inode* parent, const char* name, int inum)
{
    printf("+ directory_put(%s, %d)\n", name, inum);
    void* parent_page = pages_get_page(parent->ptrs[0]);
    for(int ii = 0; ii<4096; ii+=sizeof(dirent)) {
        dirent* current = parent_page + ii;
        if(current->inum == 0) {
            current->inum = inum;
            strncpy(current->name, name, DIR_NAME);
            parent->size += sizeof(dirent);
            return 0;
        }
    }
    return -1;
}

//deletes the given path (will also find the parent dir given the path first obvi)
int
directory_delete(const char* path)
{
    //printf(" + directory_delete(%s)\n", path);
    int parent_inum = directory_parent(path);
    inode* parent = get_inode(parent_inum);
    void* parent_page = pages_get_page(parent->ptrs[0]);
    int inum = directory_lookup(parent, path);
    inode* node = get_inode(inum);
    node->refs -= 1;
    for(int ii = 0; ii<4096; ii+=sizeof(dirent)) {
        dirent* current = parent_page + ii;
        if(streq(current->name, path)) {
            current->inum = 0;
            current->name[0] = '/';
            current->name[1] = '-';
            return 0;
        }
    }
    if(node->refs == 0) {
        free_inode(inum);
    }
    
    return 0;
}

slist*
directories_help(inode* node, const char* path)
{
    void* directory = pages_get_page(node->ptrs[0]);
    slist* toReturn = s_cons(path, 0);
    for(int ii = 0; ii<4096; ii+=sizeof(dirent)) {
        dirent* current = ((dirent*) (directory+ii));
        if(current->inum != 0) {
            if(get_inode(current->inum)->mode == 16877) {
                //printf("GOT A DIR  %s\n", current->name);
                toReturn = s_concat(directories_help(get_inode(current->inum), current->name), toReturn);
            } else {
                //printf("%s\n", current->name);
                toReturn = s_cons(current->name, toReturn);
                //printf("%d\n", get_inode(current->inum)->mode);
            }
        }
    }
    return toReturn;
}

//made this for nufs tool because I changed the purpose/implementation for directory_list
slist*
directories(const char* path) {
    int inum = get_current_root();
    inode* node = get_inode(inum);
    void* directory = pages_get_page(node->ptrs[0]);
    //slist* toReturn = s_cons(path, 0);
    slist* toReturn = 0;
    for(int ii = 0; ii<4096; ii+=sizeof(dirent)) {
        dirent* current = ((dirent*) (directory+ii));
        if(current->inum != 0) {
            if(get_inode(current->inum)->mode == 16877) {
                //printf("GOT A DIR  %s\n", current->name);
                toReturn = s_concat(directories_help(get_inode(current->inum), current->name), toReturn);
            } else {
                //printf("%s\n", current->name);
                toReturn = s_cons(current->name, toReturn);
                //printf("%d\n", get_inode(current->inum)->mode);
            }
        }
    }
    return toReturn;
}

int
tree_lookup(const char* path)
{
    //has to be
    assert(path[0] == '/');
    //inode 1 holds the page for root directory
    //COW EDIT: not anymore, root dir is determined by version num.
    if (streq(path, "/")) {
        return get_current_root();
    }

    slist* dirs = directory_list(path);
    printf(" + TREE LOOKUP %s", path);
    //keep traversing till you find the final inum
    int inum = get_current_root();
    while(dirs) {
        if(inum == -1) {
            
        }
        inode* current = get_inode(inum);
        inum = directory_lookup(current, dirs->data);
        //printf("  +  + current direcotry : %s", directories->data);
        dirs = dirs->next;
    }
    //final found
    return inum;
}

slist*
directory_list(const char* path)
{
    printf("+ directory_list(%s)\n", path);
    int ii = strlen(path) - 1;
    char current_path[DIR_NAME];
    slist* toReturn = s_cons(path, 0);
    //slist* toReturn = 0;
    while(ii > 0) {
        if(path[ii] == '/') {
            memcpy(current_path, path, ii);
            current_path[ii] = '\0';
            toReturn = s_cons(current_path, toReturn);
            //printf(" +++ directorylist -> consd %s\n", current_path);
        }
        ii--;
    }
    return toReturn;
}

// returns inum of the parent of the path that it is fed
int
directory_parent(const char* name)
{
    char parent[DIR_NAME];
    memcpy(parent, name, DIR_NAME);
    if(streq(name, "/")) {
        return get_current_root();
    }
    int length = strlen(name) - 1;
    for(int ii = length; ii >= 0; --ii) {
        if (parent[ii] == '/' && ii != 0) {
            memcpy(parent, name, ii);
            parent[ii] = '\0';
            break;
        }
        if (ii == 0) {
            memcpy(parent, name, 1);
            parent[1] = '\0';
        }
    }

    int toReturn = tree_lookup(parent);
    return toReturn;
}

//checks if a given path exists, if it does return the inum of it, if not -ENOENT
int
directory_check_path(const char* path)
{
    //If this path is root, just go on. 
    //If it's not root then its a path so check up on that parent node content
    //if can't find the path, then return error file not found.
    inode* node = get_inode(directory_parent(path));
    int path_root = strcmp(path, "/");
    if(path_root != 0) {
        if(directory_lookup(node, path) != -2) {
            return directory_lookup(node, path);
        } else {
            return -ENOENT;
        }
    }
    return directory_parent(path);
}

//copy the dirents of old directory to the dirents of new directory
int
copy_directory(inode* old_node, inode* new_node)
{
    void* old_dirents = pages_get_page(old_node->ptrs[0]);
    void* new_dirents = pages_get_page(new_node->ptrs[0]);
    int end = 4096;
    for(int ii = 0; ii<end; ii+=sizeof(dirent)) {
        dirent* old_ent = (dirent*)(old_dirents + ii);
        dirent* new_ent = (dirent*)(new_dirents + ii);
        if(old_ent->inum != 0) {
            new_ent->inum = old_ent->inum;
            strcpy(new_ent->name, old_ent->name);
            if(new_ent->inum) {
                printf("just copied %d, %s\n", new_ent->inum, new_ent->name);
            }
        }
    }

    return 0;
}

void
replace_dir_ent(inode* parent_node, const char* target_path, int new_inum, int old_inum)
{
    printf("looking to replace the path %s to new inum %d\n", target_path, new_inum);
    void* dirents = pages_get_page(parent_node->ptrs[0]);
    int end = 4096;
    for(int ii = 0; ii<end; ii+=sizeof(dirent)) {
        dirent* current = (dirent*)(dirents + ii);
        //printf("I am currently on path %s\n", current->name);
        if(current->inum != 0) {
            if(current->inum == old_inum || streq(current->name, target_path)) {
                current->inum = new_inum;
                printf("just replaced the path %s to new inum %d\n", target_path, new_inum);
            }
        }
    }
}