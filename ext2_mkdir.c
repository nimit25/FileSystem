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

char *folder_name_cutter(char *path){
  char *folder_name = malloc(sizeof(char) * (strlen(path) + 1));

  strncpy(folder_name, path, strlen(path));
  if (folder_name[strlen(folder_name) - 1] == '/') {
    folder_name[strlen(folder_name) - 1] = '\0';
  }
  folder_name = strrchr(folder_name, '/') + sizeof(char);
  return folder_name;
}

char *path_name_cutter(char *path){
    char *path_name = malloc(sizeof(char) * strlen(path));

    if (path[strlen(path) - 1] == '/') {
        path[strlen(path) - 1] = '\0';
    }
    strncpy(path_name, path, strlen(path) - strlen(strrchr(path, (int) '/')));
    snprintf(path_name, strlen(path) - strlen(strrchr(path, (int) '/')) + 1, "%s\n", path);
    return path_name;
}



int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "File usage ./ext2_mkdir <filename> <folder_path>  \n");
        exit(1);
    }

    safe_disk_mapper(argv[1]);

    if ((argc == 3 && argv[2][0] != '/') || (argc == 4 && argv[3][0] != '/')) {
        fprintf(stderr, "must be an absolute path");
        exit(1);
    }
    char *path = argv[2];
    // superblock
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE); // group descriptor

    //tests for valid file

    char *folder_name = folder_name_cutter(argv[2]);
    char *path_name = path_name_cutter(argv[2]);
    int destination_inode;


    path_walker_2(path_name, gd, &destination_inode, folder_name);




    path_walker_2(path_name, gd, &destination_inode, NULL);
//    printf("nimit test = %s %d\n", link_name, destination_inode);
    struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    struct ext2_inode *parent_inode = (struct ext2_inode *)(inode + destination_inode - 1);
    // TODO: CHECK IF IT EXISTS
    struct ext2_dir_entry_2 *new_folder;
    int inode_bit_pos = gd->bg_inode_bitmap;
    unsigned char *inode_bit_map = disk + EXT2_BLOCK_SIZE * inode_bit_pos;
    int free_inode = get_free_bit(inode_bit_map, sb->s_inodes_count/ (sizeof(unsigned char) * 8));
    if (free_inode == -1){
        fprintf(stderr, "No memory left\n");
        exit(ENOMEM);
    }
    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;

    int new_folder_inode = create_new_inode(parent_inode, folder_name, EXT2_FT_DIR, free_inode);


    struct ext2_inode *folder_inode = (struct ext2_inode *)(inode + new_folder_inode - 1);
//    for (int j=0; j< 12; j++){printf("testing2 - iblock[%d] = %d\n",j,folder_inode->i_block[j]);}



    free_inode = get_free_bit(inode_bit_map, sb->s_inodes_count/ (sizeof(unsigned char) * 8));
    if (free_inode == -1){
        fprintf(stderr, "No memory left\n");
        exit(ENOMEM);
    }
    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;
    write(".", folder_inode , folder_inode , free_inode);
    free_inode = get_free_bit(inode_bit_map, sb->s_inodes_count/ (sizeof(unsigned char) * 8));
    if (free_inode == -1){
        fprintf(stderr, "No memory left\n");
        exit(ENOMEM);
    }
    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;
    write("..", parent_inode, folder_inode , free_inode);
//    create_new_inode(parent_inode, "..", EXT2_FT_DIR);
//    create_new_inode(parent_inode, "..", EXT2_FT_DIR);

    return new_folder_inode;
}
