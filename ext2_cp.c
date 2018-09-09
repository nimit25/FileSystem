#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "helper.c"
#define CP_FLAG 1

unsigned char *disk;

FILE *safe_open(struct ext2_group_desc *gd, char *open_path){
  FILE *fp = fopen(open_path, "r");
  if (fp == NULL){
    fprintf(stderr, "Failed to open\n" );
    exit(ENOENT);
  }

  // get the size of the file we are currently reading in bytes
  fseek(fp, 0, SEEK_END);
  int file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (EXT2_BLOCK_SIZE*(gd->bg_free_blocks_count) < file_size){
    fprintf(stderr, "No more blocks left \n");
    exit(ENOSPC);
  }
  return fp;
}

char *file_name_cutter(char *path){
  //printf("strlen %d \n",(int) strlen(path));
  char *file_name = malloc(sizeof(char) * (strlen(path) + 1));

  strncpy(file_name, path, strlen(path));

  if (file_name[strlen(file_name) - 1] == '/') {
    file_name[strlen(file_name) - 1] = '\0';
  }
  file_name = strrchr(file_name, '/') + sizeof(char);

  return file_name;
}

void file_check(char *file_name, struct ext2_inode *parent_inode){
  for(int i = 0; i < 13; i++){
    int node = (parent_inode->i_block)[i];
    if (i < 12){
      if (node != 0){
	struct ext2_dir_entry_2 *curr_inode = (struct ext2_dir_entry_2 *)(disk + node*EXT2_BLOCK_SIZE);
	if (strcmp(file_name, curr_inode->name) == 0){
	  fprintf(stderr, "File already exists \n");
	  exit(EEXIST);
	}
      }
    }
    else{
      unsigned int *indirect = (unsigned int *) (disk + EXT2_BLOCK_SIZE * node);
      int j;
      for (j = 0; j < EXT2_BLOCK_SIZE / sizeof(unsigned int); j++) {
	if (indirect[j] == 1){
	  struct ext2_dir_entry_2 *ind_inode = (struct ext2_dir_entry_2 *)(disk + indirect[j]*EXT2_BLOCK_SIZE);
	  if (strcmp(file_name, ind_inode->name) == 0){
	    fprintf(stderr, "File already exists \n");
	    exit(EEXIST);
	  }
	}
      }
    }
  }
}

void copy_blocks(struct ext2_dir_entry_2 *cp_node, struct ext2_inode *base_inode, FILE *fp){
  struct ext2_inode *cp_inode = (struct ext2_inode *)((cp_node->inode)+ base_inode - 1 );
  cp_inode->i_mode = EXT2_S_IFREG;
  cp_inode->i_blocks = 0;
  cp_inode->i_dtime = 0;
  cp_inode->i_size = 0;
  cp_inode->i_blocks = 0;

  int write_complete = 0;
  int bytes_read = 0;
  int i = 0;

  while(write_complete != 1 && i < 13){
    // when direct blocks
    if (i < 12){
      (cp_inode->i_block)[i] = get_free_block();
      void *block_pointer = (void *)(disk + (cp_inode->i_block)[i] * EXT2_BLOCK_SIZE);
      bytes_read = fread(block_pointer, 1, EXT2_BLOCK_SIZE, fp);
      if (bytes_read < EXT2_BLOCK_SIZE){
         write_complete = 1;
      }
      cp_inode->i_size += bytes_read;
      i += 1;
      cp_inode->i_blocks += 1;
    }
    // if indirect block
    else{
      (cp_inode->i_block)[i] = get_free_block();
      unsigned int *indr_block_pointer = (unsigned int *) (disk + (cp_inode->i_block)[i] * EXT2_BLOCK_SIZE);


      int indr_block_size = EXT2_BLOCK_SIZE / sizeof(unsigned int);
      for (int ibk = 0; ibk < indr_block_size; ibk++){
	indr_block_pointer[ibk] = get_free_block();

	void *block_pointer1 = (void *)(disk + (indr_block_pointer)[ibk] * EXT2_BLOCK_SIZE);

	bytes_read = fread(block_pointer1, 1, EXT2_BLOCK_SIZE, fp);

	cp_inode->i_size += bytes_read;
	cp_inode->i_blocks += 1;

	if (bytes_read < EXT2_BLOCK_SIZE){
	  write_complete = 1;
	  break;
	}
      }
      cp_inode->i_blocks += 1;
    }
  }
}


struct ext2_dir_entry_2 *create_new_inode_cp(struct ext2_inode *parent_indoe, char* folder_name, unsigned char make_type){

    int k = 0;
    while (k < 12) {
        if ((parent_indoe->i_block[k] == 0) || (parent_indoe->i_block[k] == 24)){

            int block_number;
            block_number = get_free_block();
            parent_indoe->i_block[k] = block_number;

            parent_indoe->i_size += EXT2_BLOCK_SIZE;
            parent_indoe->i_blocks = EXT2_BLOCK_SIZE;
	           block_number = parent_indoe->i_block[k];

            struct ext2_dir_entry_2 *new_folder = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_number);

            // check for space for entry - is this guaranteed??
            new_folder->inode = get_free_inode();     /* Inode number */
            (inode_table_getter()[new_folder->inode]).i_links_count++;

            new_folder->rec_len = EXT2_BLOCK_SIZE;   // TODO: CAN BE SMALLER
            new_folder->name_len = strlen(folder_name);  /*Name length */
            new_folder->file_type = make_type;
            strncpy(new_folder->name, folder_name, strlen(folder_name));    /* File name, up to EXT2_NAME_LEN */
            return new_folder;
        }
        k++;
    }
    exit(ENOMEM);
}

int main(int argc, char **argv){
  if(argc != 4) {
    fprintf(stderr, "Usage: readimg <image file name> <path on computer> <path to clone to on fs>\n");
  }
  safe_disk_mapper(argv[1]);
  //struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);

  // if the same arguments are given, no need to continue
  char *cpy_path = argv[3]; // renamed argv for clarity
  if (strcmp(cpy_path, argv[2]) == 0){
    exit(0);
  }

  FILE *fp = safe_open(gd, argv[2]);

  char *file_name = file_name_cutter(argv[2]);

  struct ext2_dir_entry_2 *cp_dir = path_walker(cpy_path, gd, CP_FLAG);
  int current = 2;
  if (cp_dir != NULL){
      current = cp_dir->inode;
  }

  struct ext2_inode *base_inode = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
  struct ext2_inode *parent_inode = (struct ext2_inode *)(base_inode + current - 1);

  file_check(file_name, parent_inode);

  struct ext2_dir_entry_2 *cp_node = create_new_inode_cp(parent_inode, file_name, EXT2_FT_REG_FILE);

  copy_blocks(cp_node, base_inode, fp);
}
