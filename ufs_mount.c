#include "ufs.h"
#include "ufs_helper.h"

// 读取文件属性
// 通过路径path获取文件的属性，赋值给stbuf
static int ufs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	struct file_directory attr;

	//非根目录
	if (get_filedir(path, &attr) == -1)
	{
		puts("ufs_getattr: get filedir failed");
		return ENOENT;
	}

	memset(stbuf, 0, sizeof(struct stat));
	// 目录
	if (attr.flag == 2)
	{
		// 设置目录属性及权限
		stbuf->st_mode = S_IFDIR | 0666;
	}
	// 文件
	else if (attr.flag == 1)
	{
		// 设置文件属性及权限
		stbuf->st_mode = S_IFREG | 0666;
		// 设置文件大小
		stbuf->st_size = attr.fsize;
	}
	// 文件不存在
	else
	{
		puts("ufs_getattr：file not exist");
		res = ENOENT;
	}

	puts("ufs_getattr: end");
	return res;
}

//创建文件
static int ufs_mknod(const char *path, mode_t mode, dev_t dev)
{
	return create_filedir(path, 1);
}

//删除文件
static int ufs_unlink(const char *path)
{
	return remove_filedir(path, 1);
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

//读取路径为path的文件
static int ufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	puts("ufs_read: begin");
	struct file_directory attr;

	//读取该path所指对象的file_directory
	if (get_filedir(path, &attr) == -1)
	{
		puts("ufs_read: get filedir failed");
		return -ENOENT;
	}
	// 目录
	if (attr.flag == 2)
	{
		puts("ufs_read: can not read dir");
		return -EISDIR;
	}

	struct data_block data_blk;
	//根据文件信息读取文件内容
	if (read_block(attr.nStartBlock, &data_blk) == -1)
	{
		puts("ufs_read: read start block failed");
		return -1;
	}

	//查找文件数据块,读取并读入buf中

	if (offset < attr.fsize)
	{
		if (offset + size > attr.fsize)
			size = attr.fsize - offset;
	}
	else
		size = 0;

	int i;
	// 由于该文件可能就只有一块，所以要加以记录，后面来判断是否需要跳过block
	long check_blk = data_blk.nNextBlock;
	// 到达offset处要跳过的块的数目
	int blk_num = offset / MAX_DATA_IN_BLOCK;
	// offset所在块的偏移量
	int real_offset = offset % MAX_DATA_IN_BLOCK;

	// 跳过offset之前的block块,找到offset所指的开始位置的块
	for (i = 0; i < blk_num; i++)
	{
		if (read_block(data_blk.nNextBlock, &data_blk) == -1 || check_blk == -1)
		{
			puts("ufs_read: read block failed");
			return -1;
		}
	}

	// 文件内容跨越多个块

	// 记录size的值
	int temp = size;
	// offset所在块中需要读取的数据量
	int stb_size = 0;
	// offset所在块的数据
	char *pt = data_blk.data;
	//将数据指针移动到offset所指的位置
	pt += real_offset;

	//这里判断在offset这个文件块中要读取的字节数
	if (MAX_DATA_IN_BLOCK - real_offset < size)
		stb_size = MAX_DATA_IN_BLOCK - real_offset;
	else
		stb_size = size;

	strncpy(buf, pt, stb_size);
	temp -= stb_size;

	//把剩余未读完的内容读出来
	while (temp > 0)
	{
		if (read_block(data_blk.nNextBlock, &data_blk) == -1)
			break;					  //读到文件最后一个块就跳出
		if (temp > MAX_DATA_IN_BLOCK) //如果剩余的数据量仍大于一个块的数据量
		{
			memcpy(buf + size - temp, data_blk.data, MAX_DATA_IN_BLOCK); //从buf上一次结束的位置开始继续记录数据
			temp -= MAX_DATA_IN_BLOCK;
		}
		else //剩余的数据量少于一个块的量，说明size大小的数据读完了
		{
			memcpy(buf + size - temp, data_blk.data, temp);
			break;
		}
	}
	puts("ufs_read: end");
	return size;
}

// 写入路径为path的文件
static int ufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	puts("ufs_write: begin");
	struct file_directory attr;
	get_filedir(path, &attr);

	//然后检查要写入数据的位置是否越界
	if (offset > attr.fsize)
	{
		puts("ufs_write: offset out of range");
		return -EFBIG;
	}

	long start_blk = attr.nStartBlock;
	if (start_blk == -1)
	{
		puts("ufs_write: file is empty");
		return -errno;
	}

	int res, num;
	// 记录修改前最后一个文件块的位置
	int p_offset = offset; 
	struct data_block data_blk;

	//找到offset指定的块位置和块内的偏移量位置，注意，offset有可能很大，大于一个块的数据容量
	//而通过find_off_blk可以找到相应的文件的块位置start_blk和块内偏移位置（用offset记录）
	if ((res = find_off_blk(&start_blk, &offset, &data_blk)))
		return res;

	//创建一个指针管理数据
	char *pt = data_blk.data;
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
	data_blk.size += towrite; //该数据块的size增加已写数据量towrite
	writen += towrite;		   //buf中已写的数据量
	size -= towrite;		   //buf中待写的数据量

	//如果size>0，说明数据还没被写完，要构造空闲块作为待写入文件的新块
	long next_blk;
	if (size > 0)
	{
		//还有数据未写入，因此要找到num个连续的空闲块写入
		num = get_empty_blocks(size / MAX_DATA_IN_BLOCK + 1, &next_blk); //注意返回的是一片连续的存储空间
		//num可能小于size/MAX_DATA_IN_BLOCK+1，先写入一部分
		if (num == -1)
		{
			puts("ufs_write: get empty blocks failed");
			return -errno;
		}

		data_blk.nNextBlock = next_blk;
		//start_blk记录的是原文件的最后一个文件块，现在更新为扩大了的块
		write_block(start_blk, &data_blk);

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

				data_blk.size = towrite;
				strncpy(data_blk.data, buf, towrite);
				buf += towrite;	   //buf指针移动
				size -= towrite;   //待写入数据量减少
				writen += towrite; //已写入数据量增加

				//注意，每次写完都要检查是不是已经把整个字符串写完了，因为该文件的最后一个块的nNextBlock为-1
				if (size == 0)
					data_blk.nNextBlock = -1;
				else
					data_blk.nNextBlock = next_blk + 1; //未写完增加后续块

				//更新块为扩容后的块
				write_block(next_blk, &data_blk);
				next_blk++;
			}
			if (size == 0)
				break; //已写完
			//num小于size/504+1,找到的num不够，继续找
			num = get_empty_blocks(size / MAX_DATA_IN_BLOCK + 1, &next_blk);
			if (num == -1)
			{
				return -errno;
			}
		}
	}
	else if (size == 0) //块空间小于剩余空间
	{
		//缓存nextblock
		long next_blok = data_blk.nNextBlock;
		//终点块
		data_blk.nNextBlock = -1;
		write_block(start_blk, &data_blk);
		//清理nextblock（之前的nextblock不再需要）
		clear_blocks(next_blok, &data_blk);
	}
	size = writen;

	//修改被写文件file_directory的文件大小信息为:写入起始位置+写入内容大小
	attr.fsize = p_offset + size;
	if (set_fdir(path, &attr, 1) == -1)
		size = -errno;

	puts("ufs_write: end");
	return size;
}

//创建目录
static int ufs_mkdir(const char *path, mode_t mode)
{
	return create_filedir(path, 2);
}

//删除目录
static int ufs_rmdir(const char *path)
{
	return remove_filedir(path, 2);
}

//终端中ls -l读取目录的操作会使用到这个函数，因为fuse创建出来的文件系统是在用户空间上的
//这个函数用来读取目录，并将目录里面的文件名加入到buf里面
//步骤：① 读取path所对应目录的file_directory，找到该目录文件的nStartBlock；
//	   ② 读取nStartBlock里面的所有file_directory，并用filler把 (文件+后缀)/目录 名加入到buf里面
static int ufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) //,enum use_readdir_flags flags)
{
	struct data_block data_blk;
	struct file_directory attr;

	if (get_filedir(path, &attr) == -1) //打开path指定的文件，将文件属性读到attr中
	{
		return -ENOENT;
	}

	long start_blk = attr.nStartBlock;
	//如果该路径所指对象为文件，则返回错误信息
	if (attr.flag == 1)
	{
		return -ENOENT;
	}

	//如果path所指对象是路径，那么读出该目录的数据块内容
	if (read_block(start_blk, &data_blk))
	{
		return -ENOENT;
	}

	// 添加 . 和 ..
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//按顺序查找,并向buf添加目录内的文件和目录名
	struct file_directory *file_dir = (struct file_directory *)data_blk.data;
	int pos = 0;
	// 文件名+扩展名
	char name[MAX_FILENAME + MAX_EXTENSION + 2];
	while (pos < data_blk.size)
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
		.readdir = ufs_readdir,
	};
	return fuse_main(argc, argv, &ufs_oper, NULL);
}
