#include <stdio.h>
#include <stdlib.h>
#include "ext2.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <string.h>
#include <time.h>
unsigned char *disk;
#include "helper.c"

//char file_name[255];


void make_symlink(int inode, char *path){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    struct ext2_inode *inode1 = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    struct ext2_inode *inode2 = inode1 + inode - 1;
    int path_length = (int) strlen(path);
    int bit_pos = gd->bg_block_bitmap;
    unsigned char *bit_map_block = disk + EXT2_BLOCK_SIZE * bit_pos;
    int free_block = get_free_bit(bit_map_block, sb->s_blocks_count/ (sizeof(unsigned char) * 8));
    if (free_block == -1) {
        fprintf(stderr, "No memory left\n");
        exit(ENOMEM);
    }
    sb->s_free_blocks_count--;
    gd->bg_free_blocks_count--;
    unsigned char *block = disk + (free_block * EXT2_BLOCK_SIZE);
    memcpy(block, path, path_length);
    inode2->i_size = (unsigned int) path_length;
    inode2->i_block[0] = (unsigned int) free_block;
    inode2->i_blocks = (((inode2->i_blocks * 512) + (EXT2_BLOCK_SIZE)) / 512);
}

int main(int argc, char *argv[]){

    if (argc !=  4 && argc != 5 ){
        fprintf(stderr, "File usage: ./ext2_ln <disk name> <-s (optional)> <source file> <location>\n");
        exit(1);
    }
    disk = (unsigned char *) argv[1];
    safe_disk_mapper((char *) disk);
    char *source;
    char *destination;
    int optional_arg = 0;
    if (argc == 4){
        source = argv[2];
        destination = argv[3];
    } else {
        source = argv[3];
        destination = argv[4];
        optional_arg = 1;
    }
    if (source[0] != '/' || destination[0] != '/'){
        fprintf(stderr, "Please provide absolute paths\n");
        exit(1);
    }
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    int inode1 = get_inode(source, gd);
    int destination_inode;
    char *link_name = get_destination_name(destination, gd, &destination_inode);
    int name_len = (int) strlen(link_name);
    if (name_len > EXT2_NAME_LEN){
        exit(ENAMETOOLONG);
    }
    struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    struct ext2_inode *source_inode =  (inode + inode1 - 1);
    struct ext2_inode *destination_link = (inode + destination_inode - 1);



    if (optional_arg){ // symlink
        int inode_bit_pos = gd->bg_inode_bitmap;
        unsigned char *inode_bit_map = disk + EXT2_BLOCK_SIZE * inode_bit_pos;
        int free_inode = get_free_bit(inode_bit_map, sb->s_inodes_count/ (sizeof(unsigned char) * 8));
        if (free_inode == -1){
            fprintf(stderr, "No memory left\n");
            exit(ENOMEM);
        }
        gd->bg_free_inodes_count--;
        sb->s_free_inodes_count--;
        inode1 = free_inode;
        source_inode = (inode + inode1 - 1); // can make a helper for this -- initialize_inode()
        memset(source_inode, 0 , sizeof(struct ext2_inode));
//        unsigned int time = inode_timestamp();
        source_inode->i_mode = (unsigned short) (source_inode->i_mode | EXT2_S_IFLNK);
//        source_inode->i_atime = time;
//        source_inode->i_ctime = time;
//        source_inode->i_mtime = time;


        int directory_size = sizeof(struct ext2_dir_entry_2);
        int rec_len = get_rec_len(name_len + directory_size);
        int n = 0;
        while ( n < 12 && destination_link->i_block[n] != 0){ 
            int block_num = destination_link->i_block[n];
            unsigned char *start = disk + (block_num *EXT2_BLOCK_SIZE);
            unsigned char *last = start + EXT2_BLOCK_SIZE;




            unsigned char *start1 = start;
            while (start1 != last){
                struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) start1;
                if (dir->inode == 0){
                    if (rec_len <= dir->rec_len){
                        source_inode->i_links_count++;
                        dir->inode = (unsigned int) inode1;
                        dir->rec_len = (unsigned short) rec_len;
                        dir->name_len = (unsigned char) name_len;
                        dir->file_type = EXT2_FT_SYMLINK;
                        strncpy(dir->name, link_name,dir->name_len);
                        make_symlink(dir->inode, source);
                        return 1;
                    }
                } else{
                    int act_len = get_rec_len(directory_size + dir->name_len);
                    if (rec_len <= (dir->rec_len - act_len)){
                        struct ext2_dir_entry_2 *dir1 = dir;
                        unsigned char *next_directory = start1 + dir->rec_len;

                        dir1->rec_len = act_len;
                        start1+= act_len;
                        dir = (struct ext2_dir_entry_2 *) start1;
                        dir->inode = inode1;
                        dir->rec_len = (unsigned short) (next_directory - start1);
                        dir->name_len = name_len;
                        dir->file_type = EXT2_FT_SYMLINK;
                        strncpy(dir->name, link_name,dir->name_len);
                        make_symlink(dir->inode, source);
                        return 1;
                    }
                }
                start1 += dir->rec_len;
            }
            n++;
        }
        if ( n < 12 && (destination_link->i_block[n] == 0)){
            int bit_pos = gd->bg_block_bitmap;
            unsigned char *bit_map_block = disk + EXT2_BLOCK_SIZE * bit_pos;
            int free_block = get_free_bit(bit_map_block, sb->s_blocks_count/ (sizeof(unsigned char) * 8));
            if (free_block == -1) {
                fprintf(stderr, "No memory left\n");
                exit(ENOMEM);
            }
            sb->s_free_blocks_count--;
            gd->bg_free_blocks_count--;
            unsigned char *block = disk + (free_block * EXT2_BLOCK_SIZE);
            memset(block, 0, EXT2_BLOCK_SIZE);
            destination_link->i_block[n] = free_block;
            destination_link->i_size += EXT2_BLOCK_SIZE;
            destination_link->i_blocks =  (((destination_link->i_blocks * 512) + (EXT2_BLOCK_SIZE)) / 512);

            unsigned char *block1 = disk + (free_block * EXT2_BLOCK_SIZE);
            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) block1 ;

            int rec_len1 = (int) ((block1 + EXT2_BLOCK_SIZE) - block1);
            dir->inode = inode1;
            dir->rec_len = rec_len1;
            dir->name_len = name_len;
            dir->file_type = EXT2_FT_SYMLINK;
            strncpy(dir->name, link_name,dir->name_len);
            make_symlink(dir->inode, source);
            return 1;

        }
        fprintf(stderr, "No free memory\n");
        exit(ENOMEM);
    } else { // hardlink
        return write(link_name, source_inode, destination_link, inode1);
    }
    return 1;
}
