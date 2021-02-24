#include "ufs.h"
#include "ufs_helper.h"

//该函数用于读取文件属性（通过对象的路径获取文件的属性，并赋值给stbuf）
static int ufs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	struct file_directory *attr = malloc(sizeof(struct file_directory));

	//非根目录
	if (get_fd_to_attr(path, attr) == -1)
	{
		free(attr);
		printf("ufs_getattr：get_fd_to_attr时发生错误，函数结束返回\n\n");
		return -ENOENT;
	}

	memset(stbuf, 0, sizeof(struct stat)); //将stat结构中成员的值全部置0
	if (attr->flag == 2)
	{ //从path判断这个文件是		一个目录	还是	一般文件
		printf("ufs_getattr：这个file_directory是一个目录\n\n");
		stbuf->st_mode = S_IFDIR | 0666; //设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
										 //stbuf->st_nlink = 2;//st_nlink是连到该文件的硬连接数目,即一个文件的一个或多个文件名。说白点，所谓链接无非是把文件名和计算机文件系统使用的节点号链接起来。因此我们可以用多个文件名与同一个文件进行链接，这些文件名可以在同一目录或不同目录。
	}
	else if (attr->flag == 1)
	{
		printf("ufs_getattr：这个file_directory是一个文件\n\n");
		stbuf->st_mode = S_IFREG | 0666; //该文件是	一般文件
		stbuf->st_size = attr->fsize;
		//stbuf->st_nlink = 1;
	}
	else
	{
		printf("ufs_getattr：这个文件（目录）不存在，函数结束返回\n\n");
		;
		res = -ENOENT;
	} //文件不存在

	printf("ufs_getattr：getattr成功，函数结束返回\n\n");
	free(attr);
	return res;
}

//创建文件
static int ufs_mknod(const char *path, mode_t mode, dev_t dev)
{
	return create_file_dir(path, 1);
}

//删除文件
static int ufs_unlink(const char *path)
{
	return remove_file_dir(path, 1);
}

//打开文件时的操作
static int ufs_open(const char *path, struct fuse_file_info *fi)
{
	/*if (strcmp(path+1, options.filename) != 0)  //不是hello文件
		return -ENOENT;//文件不存在
	
	//O_ACCMODE这个宏作为一个掩码以与文件状态标识值做AND位运算，产生一个表示文件访问模式的值(其实就是去除flag的低2位)
	//这模式将是O_RDONLY, O_WRONLY, 或 O_RDWR
	if ((fi->flags & O_ACCMODE) != O_RDONLY)//只读
		return -EACCES;//权限否认（权限不足）*/

	return 0;
}

//读取文件时的操作

//根据路径path找到文件起始位置，再偏移offset长度开始读取size大小的数据到buf中，返回文件大小
//其中，buf用来存储从path读出来的文件信息，size为文件大小，offset为读取时候的偏移量，fi为fuse的文件信息
//步骤：① 先读取该path所指文件的file_directory；② 然后根据nStartBlock读出文件内容
static int ufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("ufs_read：函数开始\n\n");
	struct file_directory *attr = malloc(sizeof(struct file_directory));

	//读取该path所指对象的file_directory
	if (get_fd_to_attr(path, attr) == -1)
	{
		free(attr);
		printf("错误：ufs_read：get_fd_to_attr失败，函数结束返回\n\n");
		return -ENOENT;
	}
	//如果读取到的对象是目录，那么返回错误（只有文件会用到read这个函数）
	if (attr->flag == 2)
	{
		free(attr);
		printf("错误：ufs_read：对象为目录不是文件，读取失败，函数结束返回\n\n");
		return -EISDIR;
	}

	struct data_block *data_blk = malloc(sizeof(struct data_block));
	//根据文件信息读取文件内容
	if (read_cpy_data_block(attr->nStartBlock, data_blk) == -1)
	{
		free(attr);
		free(data_blk);
		printf("错误：ufs_read：读取文件起始块内容失败，函数结束返回\n\n");
		return -1;
	}

	//查找文件数据块,读取并读入buf中
	//要保证 读取的位置 和 加上size后的位置 在文件范围内
	if (offset < attr->fsize)
	{
		if (offset + size > attr->fsize)
			size = attr->fsize - offset;
	}
	else
		size = 0;

	int i;
	long check_blk = data_blk->nNextBlock;		  //由于该文件可能就只有一块，所以要加以记录，后面来判断是否需要跳过block
	int blk_num = offset / MAX_DATA_IN_BLOCK;	  //到达offset处要跳过的块的数目
	int real_offset = offset % MAX_DATA_IN_BLOCK; //offset所在块的偏移量

	//跳过offset之前的block块,找到offset所指的开始位置的块
	for (i = 0; i < blk_num; i++)
	{
		if (read_cpy_data_block(data_blk->nNextBlock, data_blk) == -1 || check_blk == -1)
		{
			printf("错误：ufs_read：跳到offset偏移量所在数据块时发生错误，函数结束返回\n\n");
			free(attr);
			free(data_blk);
			return -1;
		}
	}

	//文件内容可能跨多个block块，所以要用一个变量temp记录size的值，stb_size是offset所在块中需要读取的数据量
	int temp = size, stb_size = 0;
	char *pt = data_blk->data; //先读出offset所在块的数据
	pt += real_offset;		   //将数据指针移动到offset所指的位置

	//这里判断在offset这个文件块中要读取的byte数目
	if (MAX_DATA_IN_BLOCK - real_offset < size)
		stb_size = MAX_DATA_IN_BLOCK - real_offset;
	else
		stb_size = size;

	strncpy(buf, pt, stb_size);
	temp -= stb_size;

	//把剩余未读完的内容读出来
	while (temp > 0)
	{
		if (read_cpy_data_block(data_blk->nNextBlock, data_blk) == -1)
			break;					  //读到文件最后一个块就跳出
		if (temp > MAX_DATA_IN_BLOCK) //如果剩余的数据量仍大于一个块的数据量
		{
			memcpy(buf + size - temp, data_blk->data, MAX_DATA_IN_BLOCK); //从buf上一次结束的位置开始继续记录数据
			temp -= MAX_DATA_IN_BLOCK;
		}
		else //剩余的数据量少于一个块的量，说明size大小的数据读完了
		{
			memcpy(buf + size - temp, data_blk->data, temp);
			break;
		}
	}
	printf("ufs_read：文件读取成功，函数结束返回\n\n");
	free(attr);
	free(data_blk);
	return size;

	/*
	size_t len;
	(void) fi;
	if(strcmp(path+1, options.filename) != 0)//先检查这个文件存不存在
		return -ENOENT;

	len = strlen(options.contents);//读取文件的大小
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, options.contents + offset, size);
	} else
		size = 0;

	return size;*/
}

//修改文件,将buf里大小为size的内容，写入path指定的起始块后的第offset
//步骤：① 找到path所指对象的file_directory；② 根据nStartBlock和offset将内容写入相应位置；
static int ufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("ufs_write：函数开始\n\n");
	struct file_directory *attr = malloc(sizeof(struct file_directory));
	//打开path所指的对象，将其file_directory读到attr中
	get_fd_to_attr(path, attr);

	//然后检查要写入数据的位置是否越界
	if (offset > attr->fsize)
	{
		free(attr);
		printf("ufs_write：offset越界，函数结束返回\n\n");
		return -EFBIG;
	}

	long start_blk = attr->nStartBlock;
	if (start_blk == -1)
	{
		printf("ufs_write：该文件为空（无起始块），函数结束返回\n\n");
		free(attr);
		return -errno;
	}

	int res, num, p_offset = offset; //p_offset用来记录修改前最后一个文件块的位置
	struct data_block *data_blk = malloc(sizeof(struct data_block));

	//找到offset指定的块位置和块内的偏移量位置，注意，offset有可能很大，大于一个块的数据容量
	//而通过find_off_blk可以找到相应的文件的块位置start_blk和块内偏移位置（用offset记录）
	if ((res = find_off_blk(&start_blk, &offset, data_blk)))
		return res;

	//创建一个指针管理数据
	char *pt = data_blk->data;
	//找到offset所在块中offset位置
	pt += offset;

	int towrite = 0;
	int writen = 0;
	//磁盘块剩余空间小于文件大小，则写满该磁盘块剩余的数据空间，否则，说明磁盘足够大，可以写入整个文件
	if (MAX_DATA_IN_BLOCK - offset < size)
		towrite = MAX_DATA_IN_BLOCK - offset;
	else
		towrite = size;

	strncpy(pt, buf, towrite); //写入长度为towrite的内容
	buf += towrite;			   //移到字符串待写处
	data_blk->size += towrite; //该数据块的size增加已写数据量towrite
	writen += towrite;		   //buf中已写的数据量
	size -= towrite;		   //buf中待写的数据量

	//如果size>0，说明数据还没被写完，要构造空闲块作为待写入文件的新块
	long *next_blk = malloc(sizeof(long));
	if (size > 0)
	{
		//还有数据未写入，因此要找到num个连续的空闲块写入
		num = get_empty_blk(size / MAX_DATA_IN_BLOCK + 1, next_blk); //注意返回的是一片连续的存储空间
		//num可能小于size/MAX_DATA_IN_BLOCK+1，先写入一部分
		if (num == -1)
		{
			free(attr);
			free(data_blk);
			free(next_blk);
			printf("ufs_write：文件没有写完，申请空闲块失败，函数结束返回\n\n");
			return -errno;
		}

		data_blk->nNextBlock = *next_blk;
		//start_blk记录的是原文件的最后一个文件块，现在更新为扩大了的块
		write_data_block(start_blk, data_blk);

		printf("ufs_write：开始把没写完的数据写到申请到的空闲块中\n\n");
		//下面开始不断循环，把没写完的数据全部写到申请到的空闲块里面
		while (1)
		{
			for (int i = 0; i < num; i++) //重复写文件的操作（因为没写完）
			{
				//在新块写入数据，如果需要写入的剩余数据size已经不足一块的容量，那么towrite为size
				if (MAX_DATA_IN_BLOCK < size)
					towrite = MAX_DATA_IN_BLOCK;
				else
					towrite = size;

				data_blk->size = towrite;
				strncpy(data_blk->data, buf, towrite);
				buf += towrite;	   //buf指针移动
				size -= towrite;   //待写入数据量减少
				writen += towrite; //已写入数据量增加

				//注意，每次写完都要检查是不是已经把整个字符串写完了，因为该文件的最后一个块的nNextBlock为-1
				if (size == 0)
					data_blk->nNextBlock = -1;
				else
					data_blk->nNextBlock = *next_blk + 1; //未写完增加后续块

				//更新块为扩容后的块
				write_data_block(*next_blk, data_blk);
				*next_blk = *next_blk + 1;
			}
			if (size == 0)
				break; //已写完
			//num小于size/504+1,找到的num不够，继续找
			num = get_empty_blk(size / MAX_DATA_IN_BLOCK + 1, next_blk);
			if (num == -1)
			{
				free(attr);
				free(data_blk);
				free(next_blk);
				return -errno;
			}
		}
	}
	else if (size == 0) //块空间小于剩余空间
	{
		//缓存nextblock
		long next_blok = data_blk->nNextBlock;
		//终点块
		data_blk->nNextBlock = -1;
		write_data_block(start_blk, data_blk);
		//清理nextblock（之前的nextblock不再需要）
		clear_blocks(next_blok, data_blk);
	}
	size = writen;

	//修改被写文件file_directory的文件大小信息为:写入起始位置+写入内容大小
	attr->fsize = p_offset + size;
	if (setattr(path, attr, 1) == -1)
		size = -errno;

	printf("ufs_write：文件写入成功，函数结束返回\n\n");
	free(attr);
	free(data_blk);
	free(next_blk);
	return size;
}
/*
//关闭文件
static int ufs_release (const char *, struct fuse_file_info *)
{
	return 0;
}*/

//创建目录
static int ufs_mkdir(const char *path, mode_t mode)
{
	return create_file_dir(path, 2);
}

//删除目录
static int ufs_rmdir(const char *path)
{
	return remove_file_dir(path, 2);
}

//进入目录
static int ufs_access(const char *path, int flag)
{
	/*int res;
	struct file_directory* attr=malloc(sizeof(struct file_directory));
	if(get_fd_to_attr(path,attr)==-1)
	{
		free(attr);return -ENOENT;
	}
	if(attr->flag==1) {free(attr);return -ENOTDIR;}*/
	return 0;
}

//终端中ls -l读取目录的操作会使用到这个函数，因为fuse创建出来的文件系统是在用户空间上的
//这个函数用来读取目录，并将目录里面的文件名加入到buf里面
//步骤：① 读取path所对应目录的file_directory，找到该目录文件的nStartBlock；
//	   ② 读取nStartBlock里面的所有file_directory，并用filler把 (文件+后缀)/目录 名加入到buf里面
static int ufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) //,enum use_readdir_flags flags)
{
	struct data_block *data_blk;
	struct file_directory *attr;
	data_blk = malloc(sizeof(struct data_block));
	attr = malloc(sizeof(struct file_directory));

	if (get_fd_to_attr(path, attr) == -1) //打开path指定的文件，将文件属性读到attr中
	{
		free(attr);
		free(data_blk);
		return -ENOENT;
	}

	long start_blk = attr->nStartBlock;
	//如果该路径所指对象为文件，则返回错误信息
	if (attr->flag == 1)
	{
		free(attr);
		free(data_blk);
		return -ENOENT;
	}

	//如果path所指对象是路径，那么读出该目录的数据块内容
	if (read_cpy_data_block(start_blk, data_blk))
	{
		free(attr);
		free(data_blk);
		return -ENOENT;
	}

	//无论是什么目录，先用filler函数添加 . 和 ..
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//按顺序查找,并向buf添加目录内的文件和目录名
	struct file_directory *file_dir = (struct file_directory *)data_blk->data;
	int pos = 0;
	char name[MAX_FILENAME + MAX_EXTENSION + 2]; //2是因为文件名和扩展名都有nul字符
	while (pos < data_blk->size)
	{
		strcpy(name, file_dir->fname);
		if (strlen(file_dir->fext) != 0)
		{
			strcat(name, ".");
			strcat(name, file_dir->fext);
		}
		if (file_dir->flag != 0 && name[strlen(name) - 1] != '~' && filler(buf, name, NULL, 0)) //将文件名添加到buf里面
			break;
		file_dir++;
		pos += sizeof(struct file_directory);
	}
	free(attr);
	free(data_blk);
	return 0;
}

int main(int argc, char *argv[])
{
	struct fuse_operations ufs_oper = {
		.getattr = ufs_getattr,
		.mknod = ufs_mknod,
		.unlink = ufs_unlink,
		.open = ufs_open,
		.read = ufs_read,
		.write = ufs_write,
		.mkdir = ufs_mkdir,
		.rmdir = ufs_rmdir,
		.access = ufs_access,
		.readdir = ufs_readdir,
	};
	return fuse_main(argc, argv, &ufs_oper, NULL);
}
