all: init mount

init: init.c ufs.h
	gcc -o init init.c -lfuse -D_FILE_OFFSET_BITS=64

mount: mount.c ufs.h
	gcc -o mount mount.c -lfuse -D_FILE_OFFSET_BITS=64

hello:
	gcc -o hello hello.c -lfuse -D_FILE_OFFSET_BITS=64