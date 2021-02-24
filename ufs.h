#ifndef _UFS_H_
#define _UFS_H_

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

// 每块大小为512字节
#define BLOCK_SIZE 512
// 块的总数量
#define BLOCK_COUNT 10240
// 超级块数量
#define SUPER_BLOCK_COUNT 1
// bitmap块数量
#define BITMAP_BLOCK_COUNT 1280
// 根目录块数量
#define ROOT_BLOCK_COUNT 1
// 初始使用的块的数量
// 超级块+bitmap块+根目录块
#define BLOCK_USED (SUPER_BLOCK_COUNT + BITMAP_BLOCK_COUNT + ROOT_BLOCK_COUNT)
// 数据块中数据长度
// size_t和long nNextBlock各占4byte
#define MAX_DATA_IN_BLOCK (BLOCK_SIZE - 8)
// 最大文件夹数量
#define MAX_DIR_IN_BLOCK 8
// 文件名最大长度
#define MAX_FILENAME 8
// 扩展名最大长度
#define MAX_EXTENSION 3

// 超级块，用于描述整个文件系统
struct super_block{
    // 文件系统的大小，单位为块
    long fs_size;
    // 根目录的块编号
    long first_blk;
    // bitmap的大小，单位为块
    long bitmap;
};

// 文件，也可表示文件夹
struct file_directory{
    // 文件名
    char fname[MAX_FILENAME + 1];
    // 扩展名
    char fext[MAX_EXTENSION + 1];
    // 文件大小
    size_t fsize;
    // 起始块位置
    long nStartBlock;
    // 文件类型，0表示未知，1表示文件，2表示文件夹
    int flag;
};

// 数据块，保存在单个、预分配大小的diskimg文件中。
// 每块的大小为512个字节。
struct data_block{
    // 已使用的字节数
    size_t size;
    // 下一块的位置，-1表示没有下一块
    long nNextBlock;
    // 剩余空间存储数据
    char data[MAX_DATA_IN_BLOCK];
};

#endif