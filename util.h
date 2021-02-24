#include "ufs.h"

// 读取编号为idx的块
void read_block(long idx, void *block) {
    FILE *fp = NULL;
    fp = fopen(DISK_NAME, "r");
    if (fp == NULL) {
        puts("read block error: open failed");
        exit(-1);
    }
    fseek(fp, idx * BLOCK_SIZE, SEEK_SET);
    fread(block, sizeof(struct block), 1, fp);
    fclose(fp);
}


//根据文件的路径，到相应的目录寻找该文件的file_directory，并赋值给attr
void get_fd_to_attr(const char * path,struct file_directory *attr)
{
	//先要读取超级块，获取磁盘根目录块的位置
	struct super_block super_block;
	read_block(0, &super_block);
	long start_blk;
	start_blk = super_block.first_blk;

	char *tmp_path,*m,*n;//tmp_path用来临时记录路径，然后m,n两个指针是用来定位文件名和
	tmp_path=strdup(path);m=tmp_path;
	printf("check3:clear\n\n");
	//如果路径为空，则出错返回1
	if (!tmp_path) 
	{
		printf("错误：get_fd_to_attr：路径为空，函数结束返回\n\n");
		exit(-1);
	}
	
	//如果路径为根目录路径(注意这里的根目录是指/home/tootal/fuse/diskimg/???，/???之前的路径是被忽略的)
	if (strcmp(tmp_path, "/") == 0) 
	{
		attr->flag = 2;//2代表路径
		attr->nStartBlock = super_block.first_blk;
		printf("get_fd_to_attr：这是一个根目录，直接构造file_directory并返回0，函数结束返回\n\n");
		return ;
	}
	//检查完字符串既不为空也不为根目录,则要处理一下路径，而路径分为2种（我们只做2级文件系统）
	//一种是文件直接放在根目录下，则路径形为：/a.txt，另一种是在一个目录之下，则路径形为:/hehe/a.txt
	//我们路径处理的目标是让tmp_path记录diskimg下的目录名（如果path含目录的话），m记录文件名，n记录后缀名
	
	//先往后移一位，跳过第一个'/'，然后检查一下这个路径是不是有两个'/'，如果有，说明路径是/hehe/a.txt形
	m++;
	n=strchr(m,'/');
	if(n!=NULL)
	{
		printf("get_fd_to_attr：这是一个二级路径\n\n");
		tmp_path++;//此时tmp_path指着目录名的第一个字母
		*n='\0';//这样设置以后，tmp_path就变成了独立的一个字符串记录		目录名	，到'\0'停止
		n++;
		m=n;//现在m指着		文件名（含后缀名）
	}
	//如果路径不是/hehe/a.txt形，则为/a.txt形，只有一个/，所以上面的n会变成空指针
	//如果路径是/hehe/a.txt形也没有关系，依然能够让p为文件名，q为后缀名
	n=strchr(m,'.');
	if (n!=NULL) 
	{
		printf("get_fd_to_attr：检测到'.'，这是一个文件\n\n");
		*n='\0'; n++;     //q为后缀名
	}

	//读取根目录文件的信息
    struct block block;
	read_block(super_block.first_blk, &block);

	//强制类型转换，读取根目录中文件的信息，根目录块中装的都是struct file_directory
	struct file_directory *file_dir =(struct file_directory*)block.data;
	int offset=0;
	//如果path是/hehe/a.txt形路径（二级路径），那么我们先要进入到该一级目录（根目录下的目录）下
	//如果path是/a.txt形路径（一级路径），那么不会进入该分支，直接
	if (*tmp_path != '/') 
	{	//遍历根目录下所有文件的file_directory，找到该文件
		printf("get_fd_to_attr：二级路径if分支下：这是二级路径，开始寻找其父目录的nstartblock\n\n");
		while (offset < block.size) 
		{
			if (strcmp(file_dir->fname, tmp_path) == 0 && file_dir->flag == 2) //找到该目录
			{
				start_blk = file_dir->nStartBlock; break;    //记录该一级目录文件的起始块
			}
			//没找到目录继续找下一个结构
			file_dir++;
			offset += sizeof(struct file_directory);
		}
	
		//如果最后还是没找到该目录，说明目录不存在，返回-1
		if (offset == block.size) {
			printf("错误：get_fd_to_attr：二级路径if分支下的while循环内：路径错误，根目录下无此目录\n\n");return -1;
            exit(-1);
		}

		//如果找到了该目录文件的file_directory，则读出一级目录块的文件信息
        read_block(super_block.first_blk, &block);
		file_dir = (struct file_directory*) block.data;
	}
	//重置offset再进行一次file_directory的寻找操作，不过这次是直接寻找path所指文件的file_directory
	offset=0;

	printf("检查file_dir数据：file_dir:\n");
	printf("file_dir:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n",file_dir->fname,\
	file_dir->fext,file_dir->fsize,file_dir->nStartBlock,file_dir->flag);
	printf("m=%s,n=%s\n\n",m,n);

	while (offset < block.size) 
	{
		printf("check8:根据file_dir循环查找\n\n");
		//对比file_dir中的fname和fext是否与path中一致，借此找到path所指文件的file_directory
		if (file_dir->flag != 0 && strcmp(file_dir->fname, m) == 0 && (n == NULL || strcmp(file_dir->fext, n) == 0 )) 
		{ 
			//进入文件/目录所在块
			start_blk = file_dir->nStartBlock;
			//读出属性
			read_cpy_file_dir(attr, file_dir);//把找到的该文件的file_directory赋值给attr
			free(data_blk);
			printf("get_fd_to_attr：路径所指对象的父目录中匹配到了符合该path的file_directory，函数结束返回\n\n");
			return 0;
		}
		//读下一个文件
		file_dir++;
		offset += sizeof(struct file_directory);
	}
	//循环结束都还没找到，则返回-1
	printf("get_fd_to_attr：在父目录下没有找到该path的file_directory\n\n");
	free(data_blk);
	return -1;
}
