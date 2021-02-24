#ifndef _UFS_HELPER_
#define _UFS_HELPER_

struct file_directory;
struct data_block;

//根据文件的路径，到相应的目录寻找该文件的file_directory，并赋值给attr
int get_fd_to_attr(const char *path, struct file_directory *attr);

//创建path所指的文件或目录的file_directory，并为该文件（目录）申请空闲块，创建成功返回0，创建失败返回-1
//mkdir和mknod这两种操作都要用到
int create_file_dir(const char *path, int flag);

//删除path所指的文件或目录的file_directory和文件的数据块，成功返回0，失败返回-1
int remove_file_dir(const char *path, int flag);

// 该函数为读取并复制file_directory结构的内容，因为文件系统所有对文件的操作都需要先从文件所在目录
// 读取file_directory信息,然后才能找到文件在磁盘的位置，后者的数据复制给前者
void read_cpy_file_dir(struct file_directory *a, struct file_directory *b);

//根据文件的块号，从磁盘（5M大文件）中读取数据
//步骤：① 打开文件；② 将FILE指针移动到文件的相应位置；③ 读出数据块
int read_cpy_data_block(long blk_no, struct data_block *data_blk);

//根据文件的块号，将data_block结构写入到相应的磁盘块里面
//步骤：① 打开文件；② 将FILE指针移动到文件的相应位置；③写入数据块
int write_data_block(long blk_no, struct data_block *data_blk);

//路径分割，获得path所指对象的名字、后缀名，返回父目录文件的起始块位置，path是要创建的文件（目录）的目录
//步骤：① 先检查是不是二级目录（通过找第二个'/'就可以了），如果是，那么在'/'处截断字符串；
//		  如果不是，则跳过这一步
//	   ② 然后要获取这个父目录的file_directory
//	   ③ 再从path中获取文件名和后缀名（获取的时候还要判断文件名和后缀名的合法性）
//其中par_size是父目录文件的大小
int divide_path(char *name, char *ext, const char *path, long *par_dir_stblk, int flag, int *par_size);

//遍历父目录下的一个文件块内的所有文件和目录，如果已存在同名文件或目录，返回-EEXIST
//注意file_dir指针是调用函数的地方传过来的，直接++就可以了
int exist_check(struct file_directory *file_dir, char *p, char *q, int *offset, int *pos, int size, int flag);

//当一个目录文件的信息太多时（在该目录下新建文件时可能会出现该情况），为其增加后续块
int enlarge_blk(long par_dir_stblk, struct file_directory *file_dir, struct data_block *data_blk, long *tmp, char *p, char *q, int flag);

//在bitmap中标记第start_blk块是否被使用（flag=0,1)
int set_blk_use(long start_blk, int flag);

//判断该path中是否含有目录和文件，如果为空则返回1，不为空则返回0
int path_is_emp(const char *path);

//将file_directory的数据赋值给path相应文件或目录的file_directory
int setattr(const char *path, struct file_directory *attr, int flag);

//从next_blk起清空data_blk后续块
void clear_blocks(long next_blk, struct data_block *data_blk);

//根据起始块和偏移量寻找数据要写入的位置，成功返回0，失败返回-1
int find_off_blk(long *start_blk, long *offset, struct data_block *data_blk);

//找到num个连续空闲块，返回空闲块区的起始块号start_blk，返回找到的连续空闲块个数（否则返回找到的最大值）
//采用的是首次适应法（就是找到第一片大小大于num的连续区域，并将这片区域的起始块号返回）
int get_empty_blk(int num, long *start_blk);

#endif