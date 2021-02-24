#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

// FUSE API版本
// Ubuntu下安装：sudo apt install libfuse2 libfuse-dev
#define FUSE_USE_VERSION 26
#include <fuse.h>

// 磁盘文件名
#define DISK_NAME "diskimg"

// 每块的大小为512个字节
#define BLOCK_SIZE 512
// 块中数据的最大容量
// size和nNextBlock占据8字节
#define MAX_DATA_IN_BLOCK (BLOCK_SIZE - 8)
// 块的总数量
#define BLOCK_COUNT 10240
// 超级块的数量
#define SUPER_BLOCK_COUNT 1
// bitmap块的数量
#define BITMAP_BLOCK_COUNT 1280
// 初始使用的块的数量
// 超级块+bitmap块+根目录块
#define BLOCK_USED (SUPER_BLOCK_COUNT + BITMAP_BLOCK_COUNT + 1)
// 文件名的最大长度
#define MAX_FILENAME 8
// 扩展名的最大长度
#define MAX_EXTENSION 3

// 块，保存在单个、预分配大小的diskimg文件中。
// 每块的大小为512个字节。
struct block {
    // 已使用的字节数
    size_t size;
    // 下一块的位置，-1表示没有下一块
    long nNextBlock;
    // 剩余空间存储数据
    char data[MAX_DATA_IN_BLOCK];
};

// 超级块，用于描述整个文件系统
struct super_block {
    // 文件系统的大小，单位为块
    long fs_size;
    // 根目录的块编号
    long first_blk;
    // bitmap的大小，单位为块
    long bitmap;
};

// 文件，也可表示文件夹
struct file_directory {
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
