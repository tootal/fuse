#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#define DISK_PATH "/home/tootal/fuse/diskimg"

#define FS_BLOCK_SIZE 512

#define SUPER_BLOCK 1
#define BITMAP_BLOCK 1280
#define ROOT_DIR_BLOCK 1

#define MAX_DATA_IN_BLOCK 504 //size_t和long nNextBlock各占4byte
#define MAX_DIR_IN_BLOCK 8
#define MAX_FILENAME 8
#define MAX_EXTENSION 3

//超级块中记录的，大小为 24 bytes（3个long），占用1块磁盘块
struct super_block {
    long fs_size; //size of file system, in blocks（以块为单位）
    long first_blk; //first block of root directory（根目录的起始块位置，以块为单位）
    long bitmap; //size of bitmap, in blocks（以块为单位）
};

//记录文件信息的数据结构,统一存放在目录文件里面，也就是说目录文件里面存的全部都是这个结构，大小为 64 bytes，占用1块磁盘块
struct file_directory {
    char fname[MAX_FILENAME + 1]; //文件名 (plus space for nul)
    char fext[MAX_EXTENSION + 1]; //扩展名 (plus space for nul)
    size_t fsize; //文件大小（file size）
    long nStartBlock; //目录开始块位置（where the first block is on disk）
    int flag; //indicate type of file. 0:for unused; 1:for file; 2:for directory
};

//文件内容存放用到的数据结构，大小为 512 bytes，占用1块磁盘块
struct data_block {
    size_t size; //文件的数据部分使用了这个块里面的多少Bytes
    long nNextBlock; //（该文件太大了，一块装不下，所以要有下一块的地址）   long的大小为4Byte
    char data[MAX_DATA_IN_BLOCK];// And all the rest of the space in the block can be used for actual data storage.
};