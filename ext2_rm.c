#include <stdio.h>
#include <stdlib.h>
#include "ext2.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include "helper.c"


unsigned char *disk;

struct ext2_dir_entry_2 *get_directory1(int inode1, struct ext2_group_desc *gd, char *path){
    struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    struct ext2_inode *curr_inode = (struct ext2_inode *) (inode +  inode1 - 1);
    struct ext2_dir_entry_2 *dir = NULL;
    int k = 0;
    while (k < 12) {
        int block_num;
        if (curr_inode->i_block[k]) {
            block_num = curr_inode->i_block[k];
            dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
            int actual_length = 0;
            while (actual_length < EXT2_BLOCK_SIZE) {

                if (strncmp(path, dir->name, dir->name_len) == 0 && dir->name_len == strlen(path)) {
                    return dir;
                }
                actual_length = actual_length + dir->rec_len;
                dir = (void *) dir + dir->rec_len;

            }
        }
        k++;
    }
    fprintf(stderr, "Source: Not a file or directory\n");
    exit(ENOENT);
}

struct ext2_dir_entry_2 *get_parent_directory (char *path, struct ext2_group_desc *gd) {
    struct ext2_dir_entry_2 *dir = NULL;
    char *next_path;
    struct ext2_dir_entry_2 *actual_dir = get_directory1(2, gd, ".");
    path = strtok(path, "/");
    dir = get_directory1(actual_dir->inode, gd, path);


    while (path != NULL) {
        next_path = strtok(NULL, "/");
        if (next_path == NULL){
            return actual_dir;
        }
        path = next_path;
        actual_dir = dir;
        dir = get_directory1(dir->inode, gd, path);

    }
    return actual_dir;
}

void zero_inode(unsigned int inode){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    int inode_bit_pos = gd->bg_inode_bitmap;
    unsigned char *inode_bit_map = disk + EXT2_BLOCK_SIZE * inode_bit_pos;
    int index = inode / 8;
    int offset = inode % 8;
    inode_bit_map[index] = (unsigned char) (inode_bit_map[index] & (~(1 << offset)));
    gd->bg_free_inodes_count++;
    sb->s_free_inodes_count++;


}

void zero_block(unsigned int block_num){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    int bit_pos = gd->bg_block_bitmap;
    unsigned char *bit_map_block = disk + EXT2_BLOCK_SIZE * bit_pos;
    block_num = block_num - 1;
    int index = block_num / 8;
    int offset = block_num % 8;
    bit_map_block[index]  &= (~(1 << offset));
    gd->bg_free_blocks_count++;
    sb->s_free_blocks_count++;
}


int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "File usage: ./ext2_rm <disk name> <path>\n");
        exit(1);
    }
    char *path = argv[2];
    char *path1 = malloc(sizeof(char)*strlen(argv[2]) + 1);
    strcpy(path1, argv[2]);
    if (path1[0] != '/'){
        fprintf(stderr, "It should be an absolute path\n");
        exit(1);
    }
    disk = (unsigned char *) argv[1];
    safe_disk_mapper((char *) disk);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    struct ext2_dir_entry_2 *parent_dir = get_parent_directory(path, gd);
    int inode1 = get_inode(path1, gd);
    struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    struct ext2_inode *source_inode =  (inode + inode1 - 1);
    struct ext2_inode *curr_inode = (inode + parent_dir->inode -  1);
    int k = 0;
    struct ext2_dir_entry_2 *prev_dir = NULL;
    while (k < 12) {
        int block_num;
        if (curr_inode->i_block[k]) {
            block_num = curr_inode->i_block[k];
            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
            int actual_length = 0;
            while (actual_length < EXT2_BLOCK_SIZE) {
                if (strncmp(file_name, dir->name, dir->name_len) == 0 && dir->name_len == strlen(file_name)) {
                    if (source_inode->i_links_count == 0){
                            exit(1);
                    }
                    source_inode->i_links_count--;
                    if (source_inode->i_links_count == 0){
                        source_inode->i_dtime = inode_timestamp();
                        int i = 0;
                        while ( i < 12 && source_inode->i_block[i] != 0){
                            int block_num = source_inode->i_block[i];
                            zero_block((unsigned int) block_num);
                            i++;
                        }
                        if (source_inode->i_block[i] != 0){

                            unsigned int *position = (unsigned int *)( disk + EXT2_BLOCK_SIZE * source_inode->i_block[i]);
                            unsigned int n = *(position++);
                            for ( int j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int) && n != 0; j++){
                                zero_block(n);
                                n = *(position++);
                            }
                            zero_block(source_inode->i_block[i]);
                        }
                        zero_inode((unsigned int) inode1);
                    }
                    if (prev_dir == NULL){
                        dir->inode = 0;
                    }
                    else{
                        prev_dir->rec_len += dir->rec_len;
                    }
                    return 1;
                }
                actual_length = actual_length + dir->rec_len;
                prev_dir = dir;
                dir = (void *) dir + dir->rec_len;
            }
        }
        k++;
    }

    return 1;
}
