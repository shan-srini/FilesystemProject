
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "storage.h"
#include "slist.h"
#include "util.h"

slist*
image_ls_tree(const char* base)
{
    struct stat st;
    slist* zs = 0;
    slist* xs = storage_list(base);
    zs = xs;
    return zs;
}

void
print_usage(const char* name)
{
    fprintf(stderr, "Usage: %s cmd ...\n", name);
    exit(1);
}

int
main(int argc, char* argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
    }

    const char* cmd = argv[1];
    const char* img = argv[2];

    if (streq(cmd, "new")) {
        assert(argc == 3);

        storage_init(img, 1);
        printf("Created disk image: %s\n", img);
        return 0;
    }

    if (access(img, R_OK) == -1) {
        fprintf(stderr, "No such image: %s\n", img);
        return 1;
    }

    storage_init(img, 0);

    if (streq(cmd, "ls")) {
        slist* xs = image_ls_tree("/");
        printf("List for %s:\n", img);
        for (slist* it = xs; it != 0; it = it->next) {
            printf("%s\n", it->data);
        }
        s_free(xs);
        return 0;
    }

    if(streq(cmd, "versions")) {
        //run some function to get versions printed
        slist* xs = storage_versions_list();
        printf("Versions for %s:\n", img);
        for(slist* it = xs; it != 0; it = it->next) {
            printf("%s\n", it->data);
        }
        return 0;
    }

    if(streq(cmd, "rollback")) {
        assert(argc == 4);
        char* version = argv[3];
        int version_num = atoi(version);
        storage_rollback(version_num);
        //run function to rollback.
        return 0;
    }

    print_usage(argv[0]);
}
