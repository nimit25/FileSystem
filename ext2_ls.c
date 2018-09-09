#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "helper.c"


unsigned char *disk;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "File usage ./ext2_ls <filename> <-a flag (optional)> <path>  \n");
        exit(1);
    }
    safe_disk_mapper(argv[1]);

    if ((argc == 3 && argv[2][0] != '/') || (argc == 4 && argv[3][0] != '/')) {
        fprintf(stderr, "must be an absolute path");
        exit(1);
    }
    // superblock
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE); // group descriptor
    // first inode

    char *path;
    if (argc == 3) {
        path = strtok(argv[2], "/");
    } else {
        path = strtok(argv[3], "/");
    }
    int current = 2;
    struct ext2_dir_entry_2 *dir = path_walker(path, gd, 1);
    if (dir != NULL) {
        current = dir->inode;
    }

    struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);

    struct ext2_inode *main_inode = (struct ext2_inode *)(inode + current - 1);
    int k = 0;
    while (k < 12) {
        int block_num;
        if (main_inode->i_block[k]) {
            block_num = main_inode->i_block[k];

            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
            int actual_length = 0;
            while (actual_length < EXT2_BLOCK_SIZE) {
                if (argc > 3 && strcmp(argv[2], "-a") == 0){
                    printf("%.*s\n", dir->name_len, dir->name);
                }else {
                    if (strcmp(dir->name, ".") != 0 && strcmp(dir->name, "..") != 0 ){
                        printf("%.*s\n", dir->name_len, dir->name);
                    }
                }
                actual_length = actual_length + dir->rec_len;
                dir = (void *) dir + dir->rec_len;
            }
        }
        k++;
    }

    return 1;
}
