#include "ufs_helper.h"
#include "ufs.h"

int get_fd_to_attr(const char *path, struct file_directory *attr)
{
	//先要读取超级块，获取磁盘根目录块的位置
	printf("get_fd_to_attr：函数开始\n\n");
	printf("get_fd_to_attr：要寻找的file_directory的路径是%s\n\n", path);
	struct data_block *data_blk;
	data_blk = malloc(sizeof(struct data_block));
	//把超级块读出来
	if (read_cpy_data_block(0, data_blk) == -1)
	{
		printf("get_fd_to_attr：读取超级块失败，函数结束返回\n\n");
		free(data_blk);
		return -1;
	}
	struct super_block *sb_blk;
	sb_blk = (struct super_block *)data_blk;
	long start_blk;
	start_blk = sb_blk->first_blk;

	printf("检查sb_blk:\nfs_size=%ld\nfirst_blk=%ld\nbitmap=%ld\n\n", sb_blk->fs_size, sb_blk->first_blk, sb_blk->bitmap);
	printf("start_blk:%ld\n\n", start_blk);

	char *tmp_path, *m, *n; //tmp_path用来临时记录路径，然后m,n两个指针是用来定位文件名和
	tmp_path = strdup(path);
	m = tmp_path;
	//如果路径为空，则出错返回1
	if (!tmp_path)
	{
		printf("错误：get_fd_to_attr：路径为空，函数结束返回\n\n");
		free(sb_blk);
		return -1;
	}

	//如果路径为根目录路径
	if (strcmp(tmp_path, "/") == 0)
	{
		attr->flag = 2; //2代表路径
		attr->nStartBlock = start_blk;
		free(sb_blk);
		printf("get_fd_to_attr：这是一个根目录，直接构造file_directory并返回0，函数结束返回\n\n");
		return 0;
	}
	//检查完字符串既不为空也不为根目录,则要处理一下路径，而路径分为2种（我们只做2级文件系统）
	//一种是文件直接放在根目录下，则路径形为：/a.txt，另一种是在一个目录之下，则路径形为:/hehe/a.txt
	//我们路径处理的目标是让tmp_path记录diskimg下的目录名（如果path含目录的话），m记录文件名，n记录后缀名

	//先往后移一位，跳过第一个'/'，然后检查一下这个路径是不是有两个'/'，如果有，说明路径是/hehe/a.txt形
	m++;
	n = strchr(m, '/');
	if (n != NULL)
	{
		printf("get_fd_to_attr：这是一个二级路径\n\n");
		tmp_path++; //此时tmp_path指着目录名的第一个字母
		*n = '\0';	//这样设置以后，tmp_path就变成了独立的一个字符串记录		目录名	，到'\0'停止
		n++;
		m = n; //现在m指着		文件名（含后缀名）
	}
	//如果路径不是/hehe/a.txt形，则为/a.txt形，只有一个/，所以上面的n会变成空指针
	//如果路径是/hehe/a.txt形也没有关系，依然能够让p为文件名，q为后缀名
	n = strchr(m, '.');
	if (n != NULL)
	{
		printf("get_fd_to_attr：检测到'.'，这是一个文件\n\n");
		*n = '\0';
		n++; //q为后缀名
	}
	//struct data_block *data_blk=malloc(sizeof(struct data_block));

	//读取根目录文件的信息，失败的话返回-1
	if (read_cpy_data_block(start_blk, data_blk) == -1)
	{
		free(data_blk);
		printf("错误：get_fd_to_attr：读取根目录文件失败，函数结束返回\n\n");
		return -1;
	}

	//强制类型转换，读取根目录中文件的信息，根目录块中装的都是struct file_directory
	struct file_directory *file_dir = (struct file_directory *)data_blk->data;
	int offset = 0;
	//如果path是/hehe/a.txt形路径（二级路径），那么我们先要进入到该一级目录（根目录下的目录）下
	//如果path是/a.txt形路径（一级路径），那么不会进入该分支，直接
	if (*tmp_path != '/')
	{ //遍历根目录下所有文件的file_directory，找到该文件
		printf("get_fd_to_attr：二级路径if分支下：这是二级路径，开始寻找其父目录的nstartblock\n\n");
		while (offset < data_blk->size)
		{
			if (strcmp(file_dir->fname, tmp_path) == 0 && file_dir->flag == 2) //找到该目录
			{
				start_blk = file_dir->nStartBlock;
				break; //记录该一级目录文件的起始块
			}
			//没找到目录继续找下一个结构
			file_dir++;
			offset += sizeof(struct file_directory);
		}

		//如果最后还是没找到该目录，说明目录不存在，返回-1
		if (offset == data_blk->size)
		{
			free(data_blk);
			printf("错误：get_fd_to_attr：二级路径if分支下的while循环内：路径错误，根目录下无此目录\n\n");
			return -1;
		}

		//如果找到了该目录文件的file_directory，则读出一级目录块的文件信息
		if (read_cpy_data_block(start_blk, data_blk) == -1)
		{
			free(data_blk);
			printf("错误：get_fd_to_attr：二级路径if分支下：找到了二级路径父目录的nStartBlock，但是打开失败\n\n");
			return -1;
		}
		file_dir = (struct file_directory *)data_blk->data;
	}
	//重置offset再进行一次file_directory的寻找操作，不过这次是直接寻找path所指文件的file_directory
	offset = 0;

	printf("检查file_dir数据：file_dir:\n");
	printf("file_dir:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n", file_dir->fname,
		   file_dir->fext, file_dir->fsize, file_dir->nStartBlock, file_dir->flag);
	printf("m=%s,n=%s\n\n", m, n);

	while (offset < data_blk->size)
	{
		//对比file_dir中的fname和fext是否与path中一致，借此找到path所指文件的file_directory
		if (file_dir->flag != 0 && strcmp(file_dir->fname, m) == 0 && (n == NULL || strcmp(file_dir->fext, n) == 0))
		{
			//进入文件/目录所在块
			start_blk = file_dir->nStartBlock;
			//读出属性
			read_cpy_file_dir(attr, file_dir); //把找到的该文件的file_directory赋值给attr
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

int create_file_dir(const char *path, int flag)
{
	printf("调用了create_file_dir，创建的类型是：%d，创建的路径是：%s\n\n", flag, path);
	int res, par_size;
	long par_dir_blk;													 //存放父目录文件的起始块，经过exist_check后会指向父目录文件的最后一块（因为创建文件肯定实在最后一块增加file_directory的）
	char *m = malloc(15 * sizeof(char)), *n = malloc(15 * sizeof(char)); //用于存放文件名和扩展名
	//拆分路径，找到父级目录起始块
	if ((res = divide_path(m, n, path, &par_dir_blk, flag, &par_size)))
	{
		free(m);
		free(n);
		printf("错误：create_file_dir:divide_path时出错\n\n");
		return res = -1;
	}

	struct data_block *data_blk = malloc(sizeof(struct data_block));
	struct file_directory *file_dir = malloc(sizeof(struct file_directory));
	int offset = 0;
	int pos;

	while (1)
	{
		printf("create_file_dir:进入exist_check循环\n\n");
		//从目录块中读取目录信息到data_blk
		if (read_cpy_data_block(par_dir_blk, data_blk) == -1)
		{
			free(data_blk);
			free(file_dir);
			free(m);
			free(n);
			printf("错误：create_file_dir:从目录块中读取目录信息到data_blk时出错\n\n");
			return -ENOENT;
		}

		file_dir = (struct file_directory *)data_blk->data;
		offset = 0;
		pos = data_blk->size;

		//遍历父目录下的所有文件和目录，如果已存在同名文件或目录，返回-1
		//一个文件一定是连续存放的
		if ((res = exist_check(file_dir, m, n, &offset, &pos, data_blk->size, flag)))
		{
			free(data_blk);
			free(file_dir);
			free(m);
			free(n);
			printf("错误：create_file_dir:exist_check检测到该文件（目录）已存在，或出错\n\n");
			return res = -1;
		}

		if (data_blk->nNextBlock == -1 || data_blk->nNextBlock == 0)
			break;
		else
			par_dir_blk = data_blk->nNextBlock;
	}
	printf("create_file_dir:没有重名的文件或目录，开始创建文件\n\n");

	//经过exist_check函数，offset应该指向匹配到的文件的file_dir的位置，如果没找到该文件，则offset=data_blk->size
	file_dir += offset / sizeof(struct file_directory);

	long *tmp = malloc(sizeof(long));
	//假如exist_check函数里面并没有改变pos的值，那么说明flag!=0
	if (pos == data_blk->size)
	{
		printf("create_file_dir:enlarge_blk开始\n\n");
		//当前块放不下目录内容
		if (data_blk->size > MAX_DATA_IN_BLOCK)
		{
			printf("create_file_dir:当前块放不下文件，要enlarge_blk\n\n");
			//为父目录文件新增一个块
			if ((res = enlarge_blk(par_dir_blk, file_dir, data_blk, tmp, m, n, flag)))
			{
				free(data_blk);
				free(file_dir);
				free(m);
				free(n);
				printf("错误：create_file_dir:enlarge_blk出错\n\n");
				return res;
			}
			free(data_blk);
			free(file_dir);
			free(m);
			free(n);
			return 0;
		}
		else
		{ //块容量足够，直接加size
			data_blk->size += sizeof(struct file_directory);
		}
	}
	else
	{ //flag=0
		printf("create_file_dir:flag为0\n\n");
		offset = 0;
		file_dir = (struct file_directory *)data_blk->data;

		while (offset < pos)
			file_dir++;
	}
	//给新建的file_directory赋值
	strcpy(file_dir->fname, m);
	if (flag == 1 && *n != '\0')
		strcpy(file_dir->fext, n);
	file_dir->fsize = 0;
	file_dir->flag = flag;
	//找到空闲块作为起始块

	//为新建的文件申请一个空闲块
	if ((res = get_empty_blk(1, tmp)) == 1)
		file_dir->nStartBlock = *tmp;
	else
	{
		printf("错误：create_file_dir：为新建文件申请数据块时失败，函数结束返回\n\n");
		free(data_blk);
		free(file_dir);
		free(m);
		free(n);
		return -errno;
	}
	printf("tmp=%ld\n\n", *tmp);
	free(tmp);
	//将要创建的文件或目录信息写入上层目录中
	write_data_block(par_dir_blk, data_blk);
	data_blk->size = 0;
	data_blk->nNextBlock = -1;
	strcpy(data_blk->data, "\0");

	//文件起始块内容为空
	write_data_block(file_dir->nStartBlock, data_blk);

	printf("m=%s,n=%s\n\n", m, n);

	free(data_blk);
	free(m);
	free(n);
	printf("create_file_dir：创建文件成功，函数结束返回\n\n");
	return 0;
}

int remove_file_dir(const char *path, int flag)
{
	printf("remove_file_dir：函数开始\n\n");
	struct file_directory *attr = malloc(sizeof(struct file_directory));
	//读取文件属性
	if (get_fd_to_attr(path, attr) == -1)
	{
		free(attr);
		printf("错误：remove_file_dir：get_fd_to_attr失败，函数结束返回\n\n");
		return -ENOENT;
	}
	printf("检查attr:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n", attr->fname,
		   attr->fext, attr->fsize, attr->nStartBlock, attr->flag);
	//flag与指定的不一致，则返回相应错误信息
	if (flag == 1 && attr->flag == 2)
	{
		free(attr);
		printf("错误：remove_file_dir：要删除的对象flag不一致，删除失败，函数结束返回\n\n");
		return -EISDIR;
	}
	else if (flag == 2 && attr->flag == 1)
	{
		free(attr);
		printf("错误：remove_file_dir：要删除的对象flag不一致，删除失败，函数结束返回\n\n");
		return -ENOTDIR;
	}
	//清空该文件从起始块开始的后续块
	struct data_block *data_blk = malloc(sizeof(struct data_block));
	if (flag == 1)
	{
		long next_blk = attr->nStartBlock;
		clear_blocks(next_blk, data_blk);
	}
	else if (!path_is_emp(path)) //只能删除空的目录，目录非空返回错误信息
	{
		free(data_blk);
		free(attr);
		printf("remove_file_dir：要删除的目录不为空，删除失败，函数结束返回\n\n");
		return -ENOTEMPTY;
	}

	attr->flag = 0; //被删除的文件或目录的file_directory设置为未使用
	if (setattr(path, attr, flag) == -1)
	{
		printf("remove_file_dir：setattr失败，函数结束返回\n\n");
		free(data_blk);
		free(attr);
		return -1;
	}
	printf("remove_file_dir：删除成功，函数结束返回\n\n");
	free(data_blk);
	free(attr);
	return 0;
}

void read_cpy_file_dir(struct file_directory *a, struct file_directory *b)
{
    //读出属性
    strcpy(a->fname, b->fname);
    strcpy(a->fext, b->fext);
    a->fsize = b->fsize;
    a->nStartBlock = b->nStartBlock;
    a->flag = b->flag;
}

int read_cpy_data_block(long blk_no, struct data_block *data_blk)
{
    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r+");
    if (fp == NULL)
    {
        printf("错误：read_cpy_data_block：打开文件失败\n\n");
        return -1;
    }
    //文件打开成功以后，就用blk_no * BLOCK_SIZE作为偏移量
    fseek(fp, blk_no * BLOCK_SIZE, SEEK_SET);
    fread(data_blk, sizeof(struct data_block), 1, fp);
    if (ferror(fp))
    { //看读取有没有出错
        printf("错误：read_cpy_data_block：读取文件失败\n\n");
        return -1;
    }
    fclose(fp);
    return 0; //读取成功则关闭文件，然后返回0
}

int write_data_block(long blk_no, struct data_block *data_blk)
{
    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r+");
    if (fp == NULL)
    {
        printf("错误：write_data_block：打开文件失败\n\n");
        return -1;
    }
    fseek(fp, blk_no * BLOCK_SIZE, SEEK_SET);
    fwrite(data_blk, sizeof(struct data_block), 1, fp);
    if (ferror(fp))
    { //看读取有没有出错
        printf("错误：write_data_block：读取文件失败\n\n");
        return -1;
    }
    fclose(fp);
    return 0; //写入成功则关闭文件，然后返回0
}

int divide_path(char *name, char *ext, const char *path, long *par_dir_stblk, int flag, int *par_size)
{
    printf("divide_path：函数开始\n\n");
    char *tmp_path, *m, *n;
    tmp_path = strdup(path); //用来记录最原始的路径
    struct file_directory *attr = malloc(sizeof(struct file_directory));

    m = tmp_path;
    if (!m)
        return -errno; //路径为空
    m++;               //跳过第一个'/'

    n = strchr(m, '/'); //看是否有二级路径

    //如果找到二级路径的'/'，并且要创建的是目录，那么不允许，返回-1
    if (n != NULL && flag == 2)
    {
        printf("错误：divide_path:二级路径下不能再创建目录，函数结束返回-EPERM\n\n");
        return -EPERM;
    }
    else if (n != NULL)
    {
        printf("divide_path:要创建的是二级路径下的文件\n\n");
        *n = '\0';
        n++; //此时n指向要创建的文件名的第一个字母
        m = n;
        if (get_fd_to_attr(tmp_path, attr) == -1)
        { //读取该path的父目录，确认这个父目录是存在的
            printf("错误：divide_path：找不到二级路径的父目录，函数结束返回-ENOENT\n\n");
            free(attr);
            return -ENOENT;
        }
    }
    printf("divide_path检查：tmp_path=%s\nm=%s\nn=%s\n\n", tmp_path, m, n);
    //如果找不到二级路径'/'，说明要创建的对象是： /目录 或 /文件
    //那么这个路径的父目录为根目录，直接读出来
    if (n == NULL)
    {
        printf("divide_path:要创建的是根目录下的对象\n\n");
        if (get_fd_to_attr("/", attr) == -1)
        {
            printf("错误：divide_path：找不到根目录，函数结束返回-ENOENT\n\n");
            free(attr);
            return -ENOENT;
        }
    }

    //记录完要创建的对象的名字，如果该对象是文件，还要记录后缀名（有的文件没有后缀名）
    if (flag == 1)
    {
        printf("divide_path:这是文件，有后缀名\n\n");
        n = strchr(m, '.');
        if (n != NULL)
        {
            *n = '\0'; //截断tmp_path
            n++;       //此时n指针指向后缀名的第一位
        }
    }

    //要创建对象，还要检查：文件名（目录名），后缀名的长度是否超长
    if (flag == 1) //如果创建的是文件
    {
        if (strlen(m) > MAX_FILENAME + 1)
        {
            free(attr);
            return -ENAMETOOLONG;
        }
        else if (strlen(m) > MAX_FILENAME)
        {
            if (*(m + MAX_FILENAME) != '~')
            {
                free(attr);
                return -ENAMETOOLONG;
            }
        }
        else if (n != NULL) //如果有后缀名
        {
            if (strlen(n) > MAX_EXTENSION + 1)
            {
                free(attr);
                return -ENAMETOOLONG;
            }
            else if (strlen(n) > MAX_EXTENSION)
            {
                if (*(n + MAX_EXTENSION) != '~')
                {
                    free(attr);
                    return -ENAMETOOLONG;
                }
            }
        }
    }
    else if (flag == 2) //如果创建的是目录
    {
        if (strlen(m) > MAX_DIR_IN_BLOCK)
        {
            free(attr);
            return -ENAMETOOLONG;
        }
    }

    *name = '\0';
    *ext = '\0';
    if (m != NULL)
        strcpy(name, m);
    if (n != NULL)
        strcpy(ext, n);

    printf("已经获取到父目录的file_directory（attr），检查一下：\n\n");
    printf("attr:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n", attr->fname,
           attr->fext, attr->fsize, attr->nStartBlock, attr->flag);
    //把开始块信息赋值给par_dir_stblk
    *par_dir_stblk = attr->nStartBlock;
    //这里要获取父目录文件的大小
    *par_size = attr->fsize;
    printf("divide_path：检查过要创建对象的文件（目录）名，并没有问题\n\n");
    printf("divide_path：分割后的父目录名：%s\n文件名：%s\n后缀名：%s\npar_dir_stblk=%ld\n\n", tmp_path, name, ext, *par_dir_stblk);
    printf("divide_path：函数结束返回\n\n");
    free(attr);
    free(tmp_path);
    if (*par_dir_stblk == -1)
        return -ENOENT;

    return 0;
}

int exist_check(struct file_directory *file_dir, char *p, char *q, int *offset, int *pos, int size, int flag)
{
    printf("exist_check：现在开始检查该data_blk是否存在重名的对象\n\n");
    while (*offset < size)
    {
        if (flag == 0)
            *pos = *offset;
        //如果文件名、后缀名（无后缀名）皆匹配
        else if (flag == 1 && file_dir->flag == 1)
        {
            if (strcmp(p, file_dir->fname) == 0)
            {
                if ((*q == '\0' && strlen(file_dir->fext) == 0) || (*q != '\0' && strcmp(q, file_dir->fext) == 0))
                {
                    printf("错误：exist_check：存在重名的文件对象，函数结束返回-EEXIST\n\n");
                    return -EEXIST;
                }
            }
        }
        //如果目录名匹配
        else if (flag == 2 && file_dir->flag == 2 && strcmp(p, file_dir->fname) == 0)
        {
            printf("错误：exist_check：存在重名的目录对象，函数结束返回-EEXIST\n\n");
            return -EEXIST;
        }
        file_dir++;
        *offset += sizeof(struct file_directory);
    }
    printf("exist_check：函数结束返回\n\n");
    return 0;
}

int enlarge_blk(long par_dir_stblk, struct file_directory *file_dir, struct data_block *data_blk, long *tmp, char *p, char *q, int flag)
{
    printf("enlarge_blk：函数开始\n\n");
    long blk; //用于记录父目录新申请到的空闲块的位置
    tmp = malloc(sizeof(long));
    //首先我们用get_empty_blk找到一个空闲块
    if (get_empty_blk(1, tmp) == 1)
        blk = *tmp; //获取到空闲块的位置
    else
    { //如果没找到则直接返回错误信息
        printf("错误：enlarge_blk：get_empty_blk时发生错误，无法扩大文件大小，函数结束返回\n\n");
        free(p);
        free(q);
        free(data_blk);
        return -errno;
    }
    free(tmp);
    //找到的话直接给上层目录增加一个块
    data_blk->nNextBlock = blk;
    //回写父目录文件的数据
    write_data_block(par_dir_stblk, data_blk);

    //初始化新文件块的信息
    data_blk->size = sizeof(struct file_directory);
    data_blk->nNextBlock = -1;
    file_dir = (struct file_directory *)data_blk->data;
    //设置要创建的文件或目录的名字，并为这个新建的文件（目录）找到一个新的空闲块
    strcpy(file_dir->fname, p);
    if (flag == 1 && *q != '\0')
        strcpy(file_dir->fext, q);
    file_dir->fsize = 0;
    file_dir->flag = flag;
    tmp = malloc(sizeof(long));

    if (get_empty_blk(1, tmp) == 1)
        file_dir->nStartBlock = *tmp;
    else
    {
        printf("错误：enlarge_blk：get_empty_blk时发生错误，无法扩大文件大小，函数结束返回\n\n");
        return -errno;
    } //如果没找到新的空闲块，说明磁盘满了

    //此时blk仍然记录着父目录新申请到的空闲块的位置，因此我们直接把构建好的file_dir写到这个块中
    write_data_block(blk, data_blk);
    //初始化新文件的数据的信息，写入到刚刚为新文件申请到的空闲块中
    data_blk->size = 0;
    data_blk->nNextBlock = -1;
    strcpy(data_blk->data, "\0");
    write_data_block(file_dir->nStartBlock, data_blk);
    printf("enlarge_blk：扩展文件后各参数的值为：\npar_dir_stblk\n\n");
    printf("file_dir:fname=%s，fext=%s，fsize=%ld，nstartblock=%ld，flag=%d\n\n", file_dir->fname,
           file_dir->fext, file_dir->fsize, file_dir->nStartBlock, file_dir->flag);
    printf("enlarge_blk：扩展文件块成功，函数结束返回\n\n");
    return 0;
}

int set_blk_use(long start_blk, int flag)
{
    printf("set_blk_use：函数开始");
    if (start_blk == -1)
    {
        printf("错误：set_blk_use：你和我开玩笑？start_blk为-1，函数结束返回\n\n");
        return -1;
    }

    int start = start_blk / 8;      //因为每个byte有8bits，要找到start_blk在bitmap的第几个byte中，要除以8
    int left = 8 - (start_blk % 8); //计算在bitmap中的这个byte的第几位表示该块
    unsigned char f = 0x00;
    unsigned char mask = 0x01;
    mask <<= left; //构造相应位置1的掩码

    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r+");
    if (fp == NULL)
        return -1;

    fseek(fp, BLOCK_SIZE + start, SEEK_SET); //super_block占了BLOCK_SIZE个bytes
    unsigned char *tmp = malloc(sizeof(unsigned char));
    fread(tmp, sizeof(unsigned char), 1, fp); //把相应的byte读出来
    f = *tmp;

    //将该位置1，其他位不动
    if (flag)
        f |= mask;
    //将该位置0，其他位不动
    else
        f &= ~mask;

    *tmp = f;
    fseek(fp, BLOCK_SIZE + start, SEEK_SET);
    fwrite(tmp, sizeof(unsigned char), 1, fp);
    fclose(fp);
    printf("set_blk_use：bitmap状态设置成功,函数结束返回\n\n");
    free(tmp);
    return 0;
}

int path_is_emp(const char *path)
{
    printf("path_is_emp：函数开始\n\n");
    struct data_block *data_blk = malloc(sizeof(struct data_block));
    struct file_directory *attr = malloc(sizeof(struct file_directory));
    //读取属性到attr里
    if (get_fd_to_attr(path, attr) == -1)
    {
        printf("错误：path_is_emp：get_fd_to_attr失败，path所指对象的file_directory不存在，函数结束返回\n\n");
        free(data_blk);
        free(attr);
        return 0;
    }
    long start_blk;
    start_blk = attr->nStartBlock;
    //传入路径为文件
    if (attr->flag == 1)
    {
        printf("错误：path_is_emp：传入的path是文件的path，不能进行判断，函数结束返回\n\n");
        free(data_blk);
        free(attr);
        return 0;
    }
    //读出块信息
    if (read_cpy_data_block(start_blk, data_blk) == -1)
    {
        printf("错误：path_is_emp：read_cpy_data_block读取块信息失败,函数结束返回\n\n");
        free(data_blk);
        free(attr);
        return 0;
    }

    struct file_directory *file_dir = (struct file_directory *)data_blk->data;
    int pos = 0;
    //遍历目录下文件，假如存在使用中的文件或目录，非空
    while (pos < data_blk->size)
    {
        if (file_dir->flag != 0)
        {
            printf("path_is_emp：判断完毕，检测到该目录下有文件，目录不为空，函数结束返回\n\n");
            free(data_blk);
            free(attr);
            return 0;
        }
        file_dir++;
        pos += sizeof(struct file_directory);
    }
    printf("path_is_emp：判断完毕，函数结束返回\n\n");
    free(data_blk);
    free(attr);
    return 1;
}

int setattr(const char *path, struct file_directory *attr, int flag)
{
    printf("setattr：函数开始\n\n");
    int res, par_size;
    struct data_block *data_blk = malloc(sizeof(struct data_block));
    char *m = malloc(15 * sizeof(char)), *n = malloc(15 * sizeof(char));
    long start_blk;

    //path所指文件的file_directory存储在该文件所在的父目录的文件块里面，所以要进行路径分割
    if ((res = divide_path(m, n, path, &start_blk, flag, &par_size)))
    {
        printf("错误：setattr：divide_path失败，函数返回\n\n");
        free(m);
        free(n);
        return res;
    }

    //下面就是寻找文件块的位置了，当然如果目录太大有可能要继续读下一个块
    //do{
    //获取到父目录文件的起始块位置，我们就读出这个起始块
    if (read_cpy_data_block(start_blk, data_blk) == -1)
    {
        printf("错误：setattr：寻找文件块位置do_while循环内：读取文件块失败，函数结束返回\n\n");
        res = -1;
        free(data_blk);
        return res;
    }
    //start_blk=data_blk->nNextBlock;
    struct file_directory *file_dir = (struct file_directory *)data_blk->data;
    int offset = 0;
    while (offset < data_blk->size)
    {
        //循环遍历块内容
        //找到该文件
        if (file_dir->flag != 0 && strcmp(m, file_dir->fname) == 0 && (*n == '\0' || strcmp(n, file_dir->fext) == 0))
        {
            printf("setattr：找到相应路径的file_directory，可以进行回写了\n\n");
            //设置为attr指定的属性
            read_cpy_file_dir(file_dir, attr);
            res = 0;
            free(m);
            free(n);
            //将修改后的目录信息写回磁盘文件
            if (write_data_block(start_blk, data_blk) == -1)
            {
                printf("错误：setattr：while循环内回写数据块失败\n\n");
                res = -1;
            }
            free(data_blk);
            return res;
        }
        //读下一个文件
        file_dir++;
        offset += sizeof(struct file_directory);
    }
    //如果在这一个块找不到该文件，我们继续寻找该目录的下一个块，不过前提是这个目录文件有下一个块
    //}while(data_blk->nNextBlock!=-1 && data_blk->nNextBlock!=0);
    printf("setattr：赋值成功，函数结束返回\n\n");
    //找遍整个目录都没找到该文件就直接返回-1
    return -1;
}

void clear_blocks(long next_blk, struct data_block *data_blk)
{
    printf("clear_blocks：函数开始\n\n");
    while (next_blk != -1)
    {
        set_blk_use(next_blk, 0);                //在bitmap中设置相应位没有被使用
        read_cpy_data_block(next_blk, data_blk); //为了读取这个块的后续块，先读出这个块
        next_blk = data_blk->nNextBlock;
    }
    printf("clear_blocks：函数结束返回\n\n");
}

int find_off_blk(long *start_blk, long *offset, struct data_block *data_blk)
{
    printf("find_off_blk：函数开始\n\n");
    while (1)
    {
        if (read_cpy_data_block(*start_blk, data_blk) == -1)
        {
            printf("find_off_blk：读取数据块失败,函数结束返回\n\n");
            return -errno;
        }

        //offset在当前块中，说明当前文件没有跨多个块，只要在当前块找offset即可
        if (*offset <= data_blk->size)
            break;

        //否则要在后续块中找offset，先减去当前块容量（当前文件跨了很多个块）
        *offset -= data_blk->size;
        *start_blk = data_blk->nNextBlock;
    }
    printf("find_off_blk：函数结束返回\n\n");
    return 0;
}

int get_empty_blk(int num, long *start_blk)
{
    printf("get_empty_blk：函数开始\n\n");
    //从头开始找，跳过super_block、bitmap和根目录块，现在偏移量为1282（从第 0 block 偏移1282 blocks，指向编号为1282的块（其实是第1283块））
    *start_blk = 1 + BITMAP_BLOCK_COUNT + 1;
    int tmp = 0;
    //打开文件
    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r+");
    if (fp == NULL)
        return 0;
    int start, left;
    unsigned char mask, f; //8bits
    unsigned char *flag;

    //max和max_start是用来记录可以找到的最大连续块的位置
    int max = 0;
    long max_start = -1;

    //要找到一片连续的区域，我们先检查bitmap
    //要确保start_blk的合法性(一共10240块，则块编号最多去到10239)
    //每一次找不到连续的一片区域存放数据，就会不断循环，直到找到为止
    printf("get_empty_blk：现在开始寻找一片连续的区域\n\n");

    //先读超级块
    struct super_block *super_block_record = malloc(sizeof(struct super_block));
    fread(super_block_record, sizeof(struct super_block), 1, fp);
    long TOTAL_BLOCK_NUM = super_block_record->fs_size;
    while (*start_blk < TOTAL_BLOCK_NUM)
    {
        start = *start_blk / 8;      //start_blk每个循环结束都会更新到新的磁盘块空闲的位置
        left = 8 - (*start_blk % 8); //1byte里面的第几bit是空的
        mask = 1;
        mask <<= left;

        fseek(fp, BLOCK_SIZE + start, SEEK_SET);   //跳过super block，跳到bitmap中start_blk指定位置（以byte为单位）
        flag = malloc(sizeof(unsigned char));      //8bits
        fread(flag, sizeof(unsigned char), 1, fp); //读出8个bit
        f = *flag;

        //下面开始检查这一片连续存储空间是否满足num块大小的要求
        for (tmp = 0; tmp < num; tmp++)
        {
            //mask为1的位，f中为1，该位被占用，说明这一片连续空间不够大，跳出
            if ((f & mask) == mask)
                break;
            //mask最低位为1，说明这个byte已检查到最低位，8个bit已读完
            if ((mask & 0x01) == 0x01)
            { //读下8个bit
                fread(flag, sizeof(unsigned char), 1, fp);
                f = *flag;
                mask = 0x80; //指向8个bit的最高位
            }
            //位为1,右移1位，检查下一位是否可用
            else
                mask >>= 1;
        }
        //跳出上面的循环有两种可能，一种是tmp==num，说明已经找到了连续空间
        //另外一种是tmp<num说明这片连续空间不够大，要更新start_blk的位置
        //tmp为找到的可用连续块数目
        if (tmp > max)
        {
            //记录这个连续块的起始位
            max_start = *start_blk;
            max = tmp;
        }
        //如果后来找到的连续块数目少于之前找到的，不做替换
        //找到了num个连续的空白块
        if (tmp == num)
            break;

        //只找到了tmp个可用block，小于num，要重新找，更新起始块号
        *start_blk = (tmp + 1) + *start_blk;
        tmp = 0;
        //找不到空闲块
    }
    *start_blk = max_start;
    fclose(fp);
    int j = max_start;
    int i;
    //将这片连续空间在bitmap中标记为1
    for (i = 0; i < max; i++)
    {
        if (set_blk_use(j++, 1) == -1)
        {
            printf("错误：get_empty_blk：set_blk_use失败，函数结束返回\n\n");
            free(flag);
            return -1;
        }
    }
    printf("get_empty_blk：申请空间成功，函数结束返回\n\n");
    free(flag);
    return max;
}
