FILES = ext2_ls ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_rm_bonus

all: ${FILES}

ext2_ls: ext2_ls.c
	gcc -o ext2_ls ext2_ls.c 

ext2_mkdir: ext2_mkdir.c
	gcc -o ext2_mkdir ext2_mkdir.c 

ext2_cp: ext2_cp.c
	gcc -o ext2_cp ext2_cp.c 

ext2_ln: ext2_ln.c
	gcc -o ext2_ln ext2_ln.c 

ext2_rm: ext2_rm.c
	gcc -o ext2_rm ext2_rm.c 

ext2_rm_bonus: ext2_rm_bonus.c
	gcc -o ext2_rm_bonus ext2_rm_bonus.c 

clean:
	rm -f ${FILES}