#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
//#include <dirent.h>
#include <bsd/string.h>
#include <assert.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "slist.h"
#include "util.h"
#include "inode.h"
#include "directory.h"
#include "versions.h"

// implementation for: man 2 access
// Checks if a file exists.
int
nufs_access(const char *path, int mask)
{
    int rv = 0;
    printf("access(%s, %04o) -> %d\n", path, mask, rv);
    return rv;
}

// implementation for: man 2 stat
// gets an object's attributes (type, permissions, size, etc)
int
nufs_getattr(const char *path, struct stat *st)
{
    int rv = storage_stat(path, st);
    printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, rv, st->st_mode, st->st_size);
    return rv;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{       
    struct stat st;
    char item_path[128];
    int rv = 0;

    filler(buf, ".", &st, 0);
    rv = storage_stat(path, &st);
    assert(rv == 0);

    puts("****PRINTING DIRECTORY****");
    int inum = directory_check_path(path);
    printf("THIS DIRECTORY INUM %d\n", inum);
    inode* node = get_inode(inum);
    dirent* page = pages_get_page(node->ptrs[0]);
    for(int ii = 0; ii < (node->size / sizeof(dirent)); ii++) {
        dirent current = page[ii];
        if(current.inum == 0) {continue;}
        printf("dirent[%d]: %s\n", ii, current.name);
        rv = storage_stat(current.name, &st);
        //assert(rv == 0);
        for(int ii = strlen(current.name)-1; ii>=0; ii--) {
            if(current.name[ii] == '/') {
                char real_name[DIR_NAME];
                memcpy(real_name, current.name + ii + 1, strlen(current.name)-ii-1);
                real_name[strlen(current.name)-ii-1] = '\0';
                //If unused (because of delete) inum will be set to 0, reliable because true inum 0 is root
                if(current.inum == 0) {continue;}
                //printf("\n\ndecoded text!!!: %s\n\n", real_name);
                filler(buf, real_name, &st, 0);
                break;
            }
        }
    }
    //puts("\nFINISHED PRINTING DIRECOTYR");

    printf("****readdir(%s) -> %d****\n", path, rv);
    return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int rv = storage_mknod(path, mode);
    printf("+ mknod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int
nufs_mkdir(const char *path, mode_t mode)
{
    int rv = storage_mknod(path, mode | 040000);
    printf("+ mkdir(%s, %04o) -> %d\n", path, mode | 040000, rv);
    return rv;
}

int
nufs_unlink(const char *path)
{
    int rv = storage_unlink(path);
    printf("+ unlink(%s) -> %d\n", path, rv);
    return rv;
}

int
nufs_link(const char *from, const char *to)
{
    int rv = storage_link(from, to);
    printf("+ link(%s => %s) -> %d\n", from, to, rv);
	return rv;
}

int
nufs_rmdir(const char *path)
{
    //looks like rm -r is truly recursive so I don't have to implement my own recursive delete
    int rv = storage_unlink(path);
    printf("+ rmdir(%s) -> %d\n", path, rv);
    return rv;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int
nufs_rename(const char *from, const char *to)
{
    int rv = storage_rename(from, to);
    printf("+ rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
}

int
nufs_chmod(const char *path, mode_t mode)
{
    char desc[100];
    sprintf(desc, "chmod %s", path);
    new_version(path, desc);
    int inum = directory_check_path(path);
    if(inum == -ENOENT) {
        return -ENOENT;
    }
    inode* node = get_inode(inum);
    node->mode = mode;
    printf("chmod(%s, %04o) -> inum changed: %d\n", path, mode, inum);
    return 0;
}

int
nufs_truncate(const char *path, off_t size)
{
    int rv = storage_truncate(path, size);
    printf("+ truncate(%s, %ld bytes) -> %d\n", path, size, rv);
    return rv;
}

// this is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    int rv = nufs_access(path, 0);
    printf("open(%s) -> %d\n", path, rv);
    return rv;
}

// Actually read data
int
nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int rv = storage_read(path, buf, size, offset);
    printf("+ read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

// Actually write data
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int rv = storage_write(path, buf, size, offset);
    printf("+ write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

// Update the timestamps on a file or directory.
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
    //ts[0] is access and ts[1] is modify
    int rv = storage_set_time(path, ts);
    printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n",
           path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
	return rv;
}

// Extended operations
int
nufs_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi,
           unsigned int flags, void* data)
{
    int rv = -1;
    printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
    return rv;
}

int
nufs_symlink(const char* target, const char* path)
{
    char new_target[strlen(target)+3];
    if(target[0] != '/') {
        new_target[0] = '/';
        memcpy(new_target+1, target, strlen(target));
        new_target[strlen(target)+1] = '\0';
    } else {
        strcpy(new_target, target);
    }
    if (directory_check_path(new_target) == -ENOENT) {
        return -ENOENT;
    }
    char desc[100];
    sprintf(desc, "symlink target: %s path: %s", target, path);
    new_version("/", desc);
    storage_mknod(path, 0120777);
    storage_write(path, target, sizeof(path), 0);
    return 0;
}

int
nufs_readlink(const char* path, char* buf, size_t size)
{
    storage_read(path, buf, size, 0);
    return 0;
}

void
nufs_init_ops(struct fuse_operations* ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access   = nufs_access;
    ops->getattr  = nufs_getattr;
    ops->readdir  = nufs_readdir;
    ops->mknod    = nufs_mknod;
    ops->mkdir    = nufs_mkdir;
    ops->link     = nufs_link;
    ops->unlink   = nufs_unlink;
    ops->rmdir    = nufs_rmdir;
    ops->rename   = nufs_rename;
    ops->chmod    = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open	  = nufs_open;
    ops->read     = nufs_read;
    ops->write    = nufs_write;
    ops->utimens  = nufs_utimens;
    ops->ioctl    = nufs_ioctl;
    ops->symlink  = nufs_symlink;
    ops->readlink = nufs_readlink;
};

struct fuse_operations nufs_ops;

int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    storage_init(argv[--argc], 0);
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}

