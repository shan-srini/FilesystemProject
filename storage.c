
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <libgen.h>
#include <bsd/string.h>
#include <stdint.h>

#include "storage.h"
#include "slist.h"
#include "util.h"
#include "pages.h"
#include "inode.h"
#include "directory.h"
#include "versions.h"

void
storage_init(const char* path, int create)
{
    //printf("storage_init(%s, %d);\n", path, create);
    //will init page 0 for inode and bitmap AND page 1 for versions
    pages_init(path, create);
    if (create == 1) {
        //will init inode 1 and page 2 for root directory
        directory_init();
    }
}

int
storage_stat(const char* path, struct stat* st)
{
    int inum = directory_check_path(path);
    if(inum == -ENOENT) {
        return -ENOENT;
    }
    inode* node = get_inode(inum);

    memset(st, 0, sizeof(struct stat));
    st->st_uid   = getuid();
    st->st_mode  = node->mode;
    st->st_size  = node->size;
    st->st_nlink = node->refs;
    st->st_atime = node->time_access;
    st->st_mtime = node->time_modified;
    st->st_ctime = node->time_created;
    st->st_blocks = bytes_to_pages(node->size);
    printf(" ++ storage_stat -> mode: %04o size: %d \n", node->mode, node->size);
    return 0;
}

int
storage_read(const char* path, char* buf, size_t size, off_t offset)
{
    int inum = directory_check_path(path);
    if (inum == -ENOENT) {
        return -ENOENT;
    }
    inode* node = get_inode(inum);
    node->time_access = time(0);
    printf("+ storage_read(%s, offset = %ld); inode %d\n", path, offset, inum);
    //print_inode(node);

    if (offset >= node->size) {
        return 0;
    }

    size_t remaining_size = size;
    off_t offset_pos = offset;
    off_t buf_offset = 0;
    int ii = offset / 4096;

    while(remaining_size > 0) {
        int new_size = min(remaining_size, 4096 - (offset_pos % 4096));
        uint8_t* data;
        if (ii < NUM_PTRS) {
            data = pages_get_page(node->ptrs[ii]);
            data += offset_pos % 4096;
            printf(" + reading from page: %d\n", node->ptrs[ii]);
            memcpy(buf + buf_offset, data, new_size);
        } else {
            int* iptr_page = pages_get_page(node->iptr);
            data = pages_get_page(iptr_page[ii - NUM_PTRS]);
            data += offset_pos % 4096;
            printf(" + reading from indirect page: %d\n", iptr_page[ii - NUM_PTRS]);
            memcpy(buf + buf_offset, data, new_size);
        }
        //printf("THIS IS THE DATA I JUST DUMPED: %s\n\n\n", data);
        remaining_size -= new_size;
        buf_offset += new_size;
        offset_pos += new_size;
        ii++;
    }

    return size;
}

int
storage_write(const char* path, const char* buf, size_t size, off_t offset)
{
    int trv = storage_truncate(path, offset + size);
    if (trv < 0) {
        return trv;
    }
    char desc[100];
    sprintf(desc, "write %s", path);
    new_version(path, desc);
    int inum = directory_check_path(path);
    if (inum == -ENOENT) {
        return inum;
    }

    inode* node = get_inode(inum);
    node->time_modified = time(0);
    off_t offset_pos = offset;
    size_t remaining_size = size;
    int starting_page = offset / 4096;
    int ii = starting_page;
    int pos_in_buf = 0;
    while(remaining_size > 0) {
        int new_size = min(remaining_size, 4096 - (offset_pos % 4096));
        if(ii < NUM_PTRS) {
            uint8_t* data = pages_get_page(node->ptrs[ii]);
            data+=offset_pos % 4096;
            printf("+ writing to page: %d\n", node->ptrs[ii]);
            memcpy(data, buf + pos_in_buf, new_size);
        } else {
            int* iptr_page = pages_get_page(node->iptr);
            uint8_t* data = pages_get_page(iptr_page[ii - NUM_PTRS]);
            data+=offset_pos % 4096;
            printf("+ writing to page indirect: %d\n", iptr_page[ii - NUM_PTRS]);
            memcpy(data, buf + pos_in_buf, new_size);
        }
        remaining_size -= new_size;
        offset_pos += new_size;
        pos_in_buf += new_size;
        ii++;
    }

    return size;
}

int
storage_truncate(const char *path, off_t size)
{
    char desc[100];
    sprintf(desc, "truncate %s", path);
    new_version(path, desc);
    int inum = directory_check_path(path);
    if (inum < 0) {
        return inum;
    }

    inode* node = get_inode(inum);
    if(node->size < size) {
        grow_inode(node, size);
    }
    node->size = size;
    return 0;
}

int
storage_mknod(const char* path, mode_t mode)
{
    char desc[100];
    sprintf(desc, "mknod %s", path);
    new_version(path, desc);
    if (directory_check_path(path) != -ENOENT) {
        printf("mknod fail: already exist\n");
        return -EEXIST;
    }

    int    inum = alloc_inode();
    inode* node = get_inode(inum);
    node->mode = mode;
    node->size = 0;
    node->refs = 1;

    printf("+ mknod create %s [%04o] - #%d\n", path, mode, inum);
    int inum_parent = directory_parent(path);
    printf("+ mknod found parent inum %d", inum_parent);
    inode* parent = get_inode(inum_parent);
    return directory_put(parent, path, inum);
}

slist*
storage_list(const char* path)
{
    return directories(path);
}

slist*
storage_versions_list()
{
    return versions_list();
}

int
storage_unlink(const char* path)
{
    char desc[100];
    sprintf(desc, "unlink %s", path);
    new_version(path, desc);
    return directory_delete(path);
}

int
storage_link(const char* from, const char* to)
{
    char desc[100];
    sprintf(desc, "link %s", from);
    new_version(from, desc);
    int from_inum = directory_check_path(from);
    inode* from_node = get_inode(from_inum);
    from_node->refs += 1;
    inode* to_parent_node = get_inode(directory_parent(to));
    directory_put(to_parent_node, to, from_inum);
}

int
storage_rename(const char* from, const char* to)
{
    char desc[100];
    sprintf(desc, "rename from %s to %s", from, to);
    new_version(from, desc);
    int from_inum = directory_check_path(from);
    inode* node = get_inode(from_inum);
    node->refs += 1; // to prevent inode free
    directory_delete(from);
    inode* to_parent = get_inode(directory_parent(to));
    directory_put(to_parent, to, from_inum);
    return 0;
}

int
storage_set_time(const char* path, const struct timespec ts[2])
{
    char desc[100];
    sprintf(desc, "set time %s", path);
    new_version(path, desc);
    inode* node = get_inode(directory_lookup(get_inode(directory_parent(path)), path));
    node->time_access = ts[0].tv_sec;
    node->time_modified = ts[1].tv_sec;
    return 0;
}

int
storage_rollback(int rollback)
{
    return versions_rollback(rollback);
}