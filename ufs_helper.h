#ifndef _UFS_HELPER_
#define _UFS_HELPER_

#include <stdio.h>

struct file_directory;
struct super_block;
struct data_block;

// 获取路径为path的filedir到attr
int get_filedir(const char *path, struct file_directory *attr);

// 创建路径为path，类型为flag的filedir
int create_filedir(const char *path, int flag);

// 删除路径为path，类型为flag的filedir
int remove_filedir(const char *path, int flag);

// 复制file_directory结构的内容
void copy_filedir(struct file_directory *a, struct file_directory *b);

// 从磁盘文件中读取编号为idx的块
int read_block(long idx, void *block);

// 将block写入到编号为idx的磁盘块
int write_block(long idx, struct data_block *block);

// 分割路径，获得path所指对象的名字、后缀名，返回父目录文件的起始块位置，path是要创建的文件（目录）的目录
// par_size是父目录文件的大小
int divide_path(const char *path, char *name, char *ext, long *par_dir_stblk, int flag, int *par_size);

//遍历父目录下的一个文件块内的所有文件和目录，如果已存在同名文件或目录，返回-EEXIST
//注意file_dir指针是调用函数的地方传过来的，直接++就可以了
int exist_check(struct file_directory *file_dir, char *p, char *q, int *offset, int *pos, int size, int flag);

// 扩展编号为par_dir_stblk，数据为data_blk的父目录块
// 新块的文件名为filename，扩展名为extname，类型为flag
int expand_block(long par_dir_stblk,
                 struct file_directory *file_dir,
                 struct data_block *data_blk,
                 long *tmp,
                 char *filename,
                 char *extname,
                 int flag);

// 在bitmap中标记编号为idx的块是否被使用
int mark_block(long idx, int flag);

// 判断该path中是否含有目录和文件
// 如果为空则返回1，不为空则返回0
int is_empty_path(const char *path);

//将fdir的数据赋值给path相应的file_directory
int set_fdir(const char *path, struct file_directory *fdir, int flag);

// 清空编号为idx及其后续块（标记为未使用）
void clear_blocks(long idx, struct data_block *block);

//根据起始块和偏移量寻找数据要写入的位置，成功返回0，失败返回-1
int find_off_blk(long *start_blk, long *offset, struct data_block *data_blk);

// 找到num个连续空闲块，返回空闲块区的起始块号start_blk，返回找到的连续空闲块个数（否则返回找到的最大值）
// 使用首次适应法
int get_empty_blocks(int num, long *start_blk);

#endif