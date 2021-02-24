all: init

init: init.c ufs.h
	gcc -o init init.c -lfuse -D_FILE_OFFSET_BITS=64

hello:
	gcc -o hello hello.c -lfuse -D_FILE_OFFSET_BITS=64