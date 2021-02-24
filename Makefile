all: ufs_init ufs_mount

ufs_init: ufs_init.o
	gcc ufs_init.o -o ufs_init

ufs_mount: ufs_mount.o ufs_helper.o
	gcc ufs_mount.o ufs_helper.o -o ufs_mount -Wall -D_FILE_OFFSET_BITS=64 -g -pthread -lfuse -lrt -ldl

ufs_mount.o: ufs_mount.c ufs.h
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o ufs_mount.o ufs_mount.c

ufs_init.o: ufs_init.c ufs.h
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o ufs_init.o ufs_init.c

ufs_helper.o: ufs_helper.c ufs_helper.h ufs.h
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o ufs_helper.o ufs_helper.c

clean :
	rm -f ufs_mount ufs_init *.o
