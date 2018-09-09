#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <errno.h>
#include "ext2.h"
#include <linux/limits.h>
#include <string.h>
#include <time.h>

extern unsigned char *disk;
char file_name[255];



unsigned int inode_timestamp(){
    unsigned int time1 = (unsigned int) time(NULL);
    if (time1 == -1){
        exit(1);
    }
    return time1;
}




int get_rec_len(int rec_len) {
    if ((rec_len) % 4 != 0 ){
        int offset = (rec_len/4) + 1;
        rec_len = 4 * offset;
    }

    return rec_len;
}
int get_free_bit(unsigned char *bitmap, int count ) {
    for (int i = 0; i < 4; i++) {
        unsigned char nth_bit = *(bitmap + i);
        for (int j = 0; j < 32; j++, nth_bit = nth_bit >> 1) {
            if ((nth_bit & 1) == 0) {
                *(bitmap + i) = (unsigned char) (*(bitmap + i) | (1 << j));
                return (i * 8) + j + 1;
            }
        }
    }
    return -1;
}

// open disk safely
char *safe_disk_mapper(char *arg){
    int fd = open(arg, O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return 0;
}

//pathwalk - traverse each directory in the path until you reach destination file
// or directory's parent directory entry. you would need this to get the last
// file/directories inode number. also to perform error handling like ENOENT
struct ext2_dir_entry_2 *path_walker(char *path, struct ext2_group_desc *gd, int flag) {
//    int current = 2;
//    int is_dir;
//    struct ext2_dir_entry_2 *dir = NULL;
//    char *next_path;
//
//    while (path != NULL) {
//        next_path = strtok(NULL, "/");
//        struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
//        struct ext2_inode *curr_inode = (struct ext2_inode *) (inode + current -
//                                                               1); // current is the current number of the node
//        is_dir = 0;
//
//        int k = 0;
//        while (k < 12) {
//            if (is_dir == 0) {
//                int block_num;
//                if (curr_inode->i_block[k]) {
//                    block_num = curr_inode->i_block[k];
//                    dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
//                    int actual_length = 0;
//                    while (actual_length < EXT2_BLOCK_SIZE) {
//
//                        if (strncmp(path, dir->name, dir->name_len) == 0) {
//                        } else {
//                            actual_length = actual_length + dir->rec_len;
//                            dir = (void *) dir + dir->rec_len;
//                            continue;
//                        }
//                        if (dir->file_type == EXT2_FT_DIR) {
//                            current = dir->inode; // current gets updated here
//                            is_dir = 1;
//                            break;
//                        }
//                        if ((dir->file_type == EXT2_FT_REG_FILE || dir->file_type == EXT2_FT_SYMLINK) &&
//                            next_path == NULL) {
//                            printf("%.*s\n", dir->name_len, dir->name);
//                            return NULL;
//                        }
//
//                        actual_length = actual_length + dir->rec_len;
//                        dir = (void *) dir + dir->rec_len;
//
//                    }
//                }
//            }
//            k++;
//        }
//        if (is_dir == 0) {
//            fprintf(stderr, "Not a file or directory\n");
//            exit(ENOENT);
//        }
//        path = next_path;
//    }
//    return dir;
    int current = 2;
    int is_dir;
    struct ext2_dir_entry_2 *dir = NULL;
    char *next_path;
    if (flag == 1){
      path = strtok(path, "/");
    }

    while (path != NULL) {
        next_path = strtok(NULL, "/");
        struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
        struct ext2_inode *curr_inode = (struct ext2_inode *) (inode + current -
                                                               1); // current is the current number of the node
        is_dir = 0;

        int k = 0;
        while (k < 12) {
            if (is_dir == 0) {
                int block_num;
                if (curr_inode->i_block[k]) {
                    block_num = curr_inode->i_block[k];
                    dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
                    int actual_length = 0;
                    while (actual_length < EXT2_BLOCK_SIZE) {

                        if (strncmp(path, dir->name, dir->name_len) == 0) {
                        } else {
                            actual_length = actual_length + dir->rec_len;
                            dir = (void *) dir + dir->rec_len;
                            continue;
                        }
                        if (dir->file_type == EXT2_FT_DIR) {
                            current = dir->inode; // current gets updated here
                            is_dir = 1;
                            break;
                        }
                        if ((dir->file_type == EXT2_FT_REG_FILE || dir->file_type == EXT2_FT_SYMLINK) &&
                            next_path == NULL) {
                            printf("%.*s\n", dir->name_len, dir->name);
                            return NULL;
                        }

                        actual_length = actual_length + dir->rec_len;
                        dir = (void *) dir + dir->rec_len;

                    }
                }
            }
            k++;
        }
        if (is_dir == 0) {
            fprintf(stderr, "Not a file or directory\n");
            exit(ENOENT);
        }
        path = next_path;
    }
    return dir;
}

int get_inode (char *path, struct ext2_group_desc *gd) { // used in ln
    int current = 2;
    int is_dir;
    struct ext2_dir_entry_2 *dir = NULL;
    char *next_path;
    path = strtok(path, "/");

    while (path != NULL) {
        next_path = strtok(NULL, "/");
        struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
        struct ext2_inode *curr_inode = (struct ext2_inode *) (inode + current -
                                                               1); // current is the current number of the node
        is_dir = 0;

        int k = 0;
        while (k < 12 && is_dir == 0) {
            int block_num;
            if (curr_inode->i_block[k]) {
                block_num = curr_inode->i_block[k];
                dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
                int actual_length = 0;
                while (actual_length < EXT2_BLOCK_SIZE) {

                    if (strncmp(path, dir->name, dir->name_len) == 0 && dir->name_len == strlen(path)) {
                        strncpy(file_name, dir->name, dir->name_len);
                        file_name[dir->name_len] = '\0';
                        is_dir = 1;
                        current = dir->inode;
                        break;
                    }
                    actual_length = actual_length + dir->rec_len;
                    dir = (void *) dir + dir->rec_len;

                }
            }
            k++;
        }
        if (is_dir == 0) {
            fprintf(stderr, "Source: Not a file or directory\n");
            exit(ENOENT);
        }
        path = next_path;
    }
    if (dir != NULL && dir->file_type == EXT2_FT_DIR){
        fprintf(stderr, "It should be a file\n");
        exit(EISDIR);
    }
    return current;
}

char *get_destination_name(char *path, struct ext2_group_desc *gd, int *current_node) { // used in ln
    int current = 2;
    int is_dir;
    int counter = 0;
    struct ext2_dir_entry_2 *dir = NULL;
    char *next_path;
    path = strtok(path, "/");

    while (path != NULL) {
        counter++;
        next_path = strtok(NULL, "/");
        struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
        struct ext2_inode *curr_inode = (struct ext2_inode *) (inode + current -
                                                               1); // current is the current number of the node
        is_dir = 0;

        int k = 0;
        while (k < 12 && is_dir == 0) {
            int block_num;
            if (curr_inode->i_block[k]) {
                block_num = curr_inode->i_block[k];
                dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
                int actual_length = 0;
                while (actual_length < EXT2_BLOCK_SIZE) {

                    if (strncmp(path, dir->name, dir->name_len) == 0 && dir->name_len == strlen(path)) {
                        is_dir = 1;
                        current = dir->inode;
                        break;
                    }
                    actual_length = actual_length + dir->rec_len;
                    dir = (void *) dir + dir->rec_len;

                }
            }
            k++;
        }
        if (next_path == NULL && is_dir && dir->file_type != EXT2_FT_DIR){
            fprintf(stderr, "File exists - '%s'\n", path); // TODO REVERT
            exit(EEXIST);
        }
        if (next_path == NULL && is_dir && dir->file_type == EXT2_FT_DIR){
            next_path = file_name;
        }
        if (next_path != NULL && !is_dir){
            fprintf(stderr, "invalid path\n");
            exit(EINVAL);
        }
        if (next_path == NULL){
            break;
        }
        path = next_path;
    }
    *current_node = current;
    return path;
}

int write(char *link_name, struct ext2_inode *source_inode,
    struct ext2_inode *destination_link, int inode1){
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    int name_len = (int) strlen(link_name);

    int directory_size = sizeof(struct ext2_dir_entry_2);
    int rec_len = get_rec_len(name_len + directory_size);
    int n = 0;
    while ( n < 12 && destination_link->i_block[n] != 0){
        int block_num = destination_link->i_block[n];
        unsigned char *start = disk + (block_num *EXT2_BLOCK_SIZE);
        unsigned char *block_end = start + EXT2_BLOCK_SIZE;

        unsigned char *start1 = start;
        while (start1 != block_end){
            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) start1;
            if (dir->inode == 0){
                if (rec_len <= dir->rec_len){
                    source_inode->i_links_count++;
                    dir->inode = inode1;
                    dir->rec_len = rec_len;
                    dir->name_len = name_len;
                    dir->file_type = EXT2_FT_REG_FILE;
                    strncpy(dir->name, link_name,dir->name_len);
                    return 1;
                }

            } else{
                int act_len = get_rec_len(directory_size + dir->name_len);
                if (rec_len <= (dir->rec_len - act_len)){
                    struct ext2_dir_entry_2 *previous = dir;
                    unsigned char *next = start1 + dir->rec_len;

                    previous->rec_len = act_len;
                    start1+= act_len;
                    dir = (struct ext2_dir_entry_2 *) start1;
                    dir->inode = inode1;
                    dir->rec_len = (unsigned short) (next - start1);
                    dir->name_len = name_len;
                    dir->file_type = EXT2_FT_REG_FILE;
                    strncpy(dir->name, link_name,dir->name_len);
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
        struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) block1;
        int rec_len1 = (int) ((block1 + EXT2_BLOCK_SIZE) - block1);
        dir->inode = inode1;
        dir->rec_len = rec_len1;
        dir->name_len = name_len;
        dir->file_type = EXT2_FT_REG_FILE;
        strncpy(dir->name, link_name,dir->name_len);
        return 1;
    }











    fprintf(stderr, "No free memory\n");
    exit(ENOMEM);
}

struct ext2_inode *inode_table_getter() {
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE); // group descriptor
    return (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
}

int freespot_bitmap(char* bitmap, int num_inode_bit){
    int i, j, k;
    j = 0;
    for (i = 0; i < num_inode_bit/(sizeof(unsigned char) * 8); i++) {

        /* Looping through each bit in a byte and print the least
         * significant bit first. That is reverse the order of the bit. */
        for (k = 0; k < 8; k++) {
//            printf("%d\n", (bitmap[i] >> k) & 1);
            if (0 == ((bitmap[i] >> k) & 1)){
                return j;
            };
            j++;
        }
    }
    exit(ENOMEM);
}

//sets first free bit to high and returns #
int bitmap_setter(unsigned char* bitmap, int size){
    int k =0;
    while (k < size){
        int j = 0;
        while (j < 8) {
//            printf("%d\n", (bitmap[k] >> j) & 1);
            if (0 == ((bitmap[k] >> j) & 1)){
                bitmap[k] |= 1<< j;
                return (int) ((k * 8) + j);
            }
            j++;
        }

        k++;
    }
    exit(ENOMEM);
}
// creates and returns a new inode
int get_free_inode(){

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);

    int free_inode = -1;
    unsigned char *bitmap = disk + EXT2_BLOCK_SIZE * (gd->bg_inode_bitmap);
    int k = 10;
    while (k < sb->s_inodes_count){
        if (((1 << (k % 8)) & (bitmap[k >> 3])) == 0){
            bitmap[k >> 3] |= 1 << (k % 8);
            free_inode = k + 1;
            break;
        }
        k++;
    }
    if (free_inode == -1){
        fprintf(stderr, "No memory left\n");
        exit(ENOMEM);
    }

    struct ext2_inode *new_inode = &(inode_table_getter()[free_inode]);
    memset(new_inode, 0, sizeof(struct ext2_inode));

    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;

    return free_inode;
}

int get_free_block(){
    // superblock
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE); // group descriptor

    unsigned char *bitmap = disk + EXT2_BLOCK_SIZE * (gd->bg_block_bitmap);
    int free_block = bitmap_setter(bitmap, sb->s_blocks_count) + sb->s_first_data_block;
    sb->s_free_blocks_count--;
    gd->bg_free_blocks_count--;

    unsigned char *block = disk + (free_block * EXT2_BLOCK_SIZE);
    memset(block, 0, EXT2_BLOCK_SIZE);

    return free_block;
}


int create_new_inode(struct ext2_inode *parent_indoe, char* folder_name, unsigned char make_type, int inode){
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    int name_len = (int) strlen(folder_name);
    int rec_len = get_rec_len(name_len + sizeof(sizeof(struct ext2_dir_entry_2)));
//    printf("test2\n");
    int k = 0;
    while ( k < 12 && parent_indoe->i_block[k] != 0){
        int block_num = parent_indoe->i_block[k];
        unsigned char *start = disk + (block_num *EXT2_BLOCK_SIZE);
        unsigned char *block_end = start + EXT2_BLOCK_SIZE;

        unsigned char *start1 = start;
        while (start1 != block_end){
            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) start1;
//            printf("test5, '%s' %d\n", dir->name, block_num);
//            if (strlen(dir->name) == 0){exit(1);}
            if (dir->inode == 0){
                if (rec_len <= dir->rec_len){
                    parent_indoe->i_links_count++;
                    dir->inode = inode;
                    dir->rec_len = rec_len;
                    dir->name_len = name_len;
                    dir->file_type = make_type;
                    strncpy(dir->name, folder_name,dir->name_len);
                    gd->bg_used_dirs_count++;
                    return dir->inode;
                }

            } else{
//                printf("test4\n");
                int act_len = get_rec_len(sizeof(struct ext2_dir_entry_2) + dir->name_len);
                if (rec_len <= (dir->rec_len - act_len)){
                    struct ext2_dir_entry_2 *previous = dir;
                    unsigned char *next = start1 + dir->rec_len;

                    previous->rec_len = act_len;
                    start1+= act_len;
                    dir = (struct ext2_dir_entry_2 *) start1;
                    dir->inode = inode;
                    dir->rec_len = (unsigned short) (next - start1);
                    dir->name_len = name_len;
                    dir->file_type = make_type;
                    strncpy(dir->name, folder_name,dir->name_len);
                    gd->bg_used_dirs_count++;
//                    printf("test3\n");

                    return dir->inode;
                }
            }
            start1 += dir->rec_len;
        }
        k++;
    }
        if ( k < 12 && (parent_indoe->i_block[k] == 0)){

            int block_number;
            block_number = get_free_block();
            parent_indoe->i_block[k] = block_number;

            parent_indoe->i_size += EXT2_BLOCK_SIZE;
            parent_indoe->i_blocks =  (((parent_indoe->i_blocks * 512) + (EXT2_BLOCK_SIZE)) / 512);

            struct ext2_inode *inode_table = inode_table_getter();

            block_number = parent_indoe->i_block[k];

            struct ext2_dir_entry_2 *new_folder = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_number);
            int actual_length = 0;

            new_folder->inode = inode;     /* Inode number */
            (inode_table_getter()[new_folder->inode]).i_links_count++;
            new_folder->rec_len = rec_len;
            new_folder->name_len = strlen(folder_name);  /*Name length */
            new_folder->file_type = make_type;
            strncpy(new_folder->name, folder_name, strlen(folder_name));    /* File name, up to EXT2_NAME_LEN */
            gd->bg_used_dirs_count++;
            new_folder->inode ;
            return new_folder->inode;

    }
    exit(ENOMEM);
}

// if not doing mkdir folder_name should be null
struct ext2_dir_entry_2 *path_walker_2(char *path, struct ext2_group_desc *gd, int *current_node, char *folder_name) { // used in ln
    int current = 2;
    int is_dir;
    int counter = 0;
    struct ext2_dir_entry_2 *dir = NULL;
    char *next_path;
    path = strtok(path, "/");
    int checking_mode = 0;
    if (folder_name != NULL && path == NULL){
        path = folder_name;
        checking_mode = 1;
    }
    while (path != NULL) {
        counter++;
        next_path = strtok(NULL, "/");
        struct ext2_inode *inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
        struct ext2_inode *curr_inode = (struct ext2_inode *) (inode + current -
                                                               1); // current is the current number of the node
        is_dir = 0;

        int k = 0;
        while (k < 12 && is_dir == 0) {
            int block_num;
            if (curr_inode->i_block[k]) {
                block_num = curr_inode->i_block[k];
                dir = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_num);
                int actual_length = 0;
                while (actual_length < EXT2_BLOCK_SIZE) {
                    if (strncmp(path, dir->name, dir->name_len) == 0 &&
                            dir->name_len == strlen(path) && checking_mode == 0) {
                        is_dir = 1;
                        current = dir->inode;
                        break;
                    }
                    if ((folder_name != NULL) &&(strncmp(folder_name, dir->name, dir->name_len) == 0)
                        && dir->name_len == strlen(path)){
                        fprintf(stderr, "Folder exists\n"); //
                        exit(EEXIST);
                    }
                    actual_length = actual_length + dir->rec_len;
                    dir = (void *) dir + dir->rec_len;

                }
            }
            k++;
        }
        if (folder_name != NULL){
            if(checking_mode == 0 && strncmp(folder_name, path, strlen(path)) == 0 && strlen(path) == strlen(folder_name) && next_path == NULL && strlen(path) != 0){
                fprintf(stderr, "Folder exists *** %s\n", folder_name); // TODO REVERT
                exit(EEXIST);
            }
        }
        if (next_path != NULL && !is_dir){
            fprintf(stderr, "invalid path\n");
            exit(EINVAL);
        }
        if (next_path == NULL){
            break;
        }
        path = next_path;
    }
    if (checking_mode == 1){
        *current_node = 2;
        return NULL;
    }
    *current_node = current;

    return dir;
}
