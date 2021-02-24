all: init mount

init: init.o
	gcc init.o -o init

mount: mount.o
	gcc mount.o -o mount -Wall -D_FILE_OFFSET_BITS=64 -g -pthread -lfuse -lrt -ldl

mount.o: mount.c ufs.h
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o mount.o mount.c

init.o: init.c ufs.h
	gcc -Wall -D_FILE_OFFSET_BITS=64 -g -c -o init.o init.c

clean :
	rm -f mount init mount.o init.o
