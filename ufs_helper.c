#include "ufs_helper.h"
#include "ufs.h"

int get_filedir(const char *path, struct file_directory *attr)
{
    //先要读取超级块，获取磁盘根目录块的位置
    struct super_block super_block;
    //把超级块读出来
    if (read_block(0, &super_block) == -1)
    {
        puts("get_filedir: read super block failed");
        return -1;
    }
    long start_blk = super_block.first_blk;

    char *tmp_path;
    // 指向文件名
    char *m;
    // 指向后缀名
    char *n;
    tmp_path = strdup(path);
    m = tmp_path;

    if (!tmp_path)
    {
        puts("get_filedir: path is empty");
        return -1;
    }

    //如果路径为根目录路径
    if (strcmp(tmp_path, "/") == 0)
    {
        attr->flag = 2;
        attr->nStartBlock = start_blk;
        puts("get_filedir: path is root");
        return 0;
    }
    
    // 跳过第一个'/'
    m++;
    n = strchr(m, '/');
    // 包含两个'/'，说明是二级路径
    if (n != NULL)
    {
        tmp_path++; //此时tmp_path指向目录名的第一个字母
        *n = '\0';  //这样设置以后，tmp_path就变成了独立的一个字符串记录目录名，到'\0'停止
        n++;
        m = n; //现在m指着文件名（含后缀名）
    }
    //如果路径不是/hehe/a.txt形，则为/a.txt形，只有一个/，所以上面的n会变成空指针
    //如果路径是/hehe/a.txt形也没有关系，依然能够让p为文件名，q为后缀名
    n = strchr(m, '.');
    if (n != NULL)
    {
        puts("get_filedir: this is a file");
        *n = '\0';
        n++;
    }

    struct data_block block;

    //读取根目录文件的信息，失败的话返回-1
    if (read_block(start_blk, &block) == -1)
    {
        puts("get_filedir: read root block error");
        return -1;
    }

    //强制类型转换，读取根目录中文件的信息，根目录块中装的都是struct file_directory
    struct file_directory *file_dir = (struct file_directory *)block.data;
    int offset = 0;
    //如果path是/hehe/a.txt形路径（二级路径），那么我们先要进入到该一级目录（根目录下的目录）下
    if (*tmp_path != '/')
    { //遍历根目录下所有文件的file_directory，找到该文件
        while (offset < block.size)
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

        // 目录不存在
        if (offset == block.size)
        {
            puts("get_filedir: path error, can not find dir");
            return -1;
        }

        // 读出一级目录块的文件信息
        if (read_block(start_blk, &block) == -1)
        {
            puts("get_filedir: read block error");
            return -1;
        }
        file_dir = (struct file_directory *)block.data;
    }

    // 查找文件
    offset = 0;

    while (offset < block.size)
    {
        if (file_dir->flag != 0 && strcmp(file_dir->fname, m) == 0 && (n == NULL || strcmp(file_dir->fext, n) == 0)) // 允许空后缀
        {
            //进入文件/目录所在块
            start_blk = file_dir->nStartBlock;
            //读出属性
            copy_filedir(attr, file_dir);
            puts("get_filedir: end");
            return 0;
        }
        //读下一个文件
        file_dir++;
        offset += sizeof(struct file_directory);
    }
    // 找不到文件
    puts("get_filedir: can not find file");
    return -1;
}

int create_filedir(const char *path, int flag)
{
    puts("create_filedir: begin");
    int res, par_size;
    // 父目录文件的起始块
    long par_dir_blk;
    char filename[MAX_FILENAME + 1];
    char extname[MAX_EXTENSION + 1];
    //拆分路径，找到父级目录起始块
    if ((res = divide_path(path, filename, extname, &par_dir_blk, flag, &par_size)))
    {
        puts("create_filedir: divide_path error");
        return -1;
    }

    struct data_block *data_blk = malloc(sizeof(struct data_block));
    struct file_directory *file_dir = malloc(sizeof(struct file_directory));
    int offset = 0;
    int pos;

    while (1)
    {
        //从目录块中读取目录信息到data_blk
        if (read_block(par_dir_blk, data_blk) == -1)
        {
            puts("create_filedir: read block error");
            return -ENOENT;
        }

        file_dir = (struct file_directory *)data_blk->data;
        offset = 0;
        pos = data_blk->size;

        //遍历父目录下的所有文件和目录，如果已存在同名文件或目录，返回-1
        //一个文件一定是连续存放的
        if ((res = exist_check(file_dir, filename, extname, &offset, &pos, data_blk->size, flag)))
        {
            free(data_blk);
            free(file_dir);
            puts("create_filedir: file already exist");
            return -1;
        }

        if (data_blk->nNextBlock == -1 || data_blk->nNextBlock == 0)
            break;
        else
            par_dir_blk = data_blk->nNextBlock;
    }

    // 经过exist_check函数，offset指向匹配到的文件的file_dir的位置
    file_dir += offset / sizeof(struct file_directory);

    long tmp;
    //exist_check函数没有改变pos的值，那么说明flag!=0
    if (pos == data_blk->size)
    {
        // 当前块放不下目录内容
        if (data_blk->size > MAX_DATA_IN_BLOCK)
        {
            //为父目录文件新增一个块
            if ((res = expand_block(par_dir_blk, file_dir, data_blk, &tmp, filename, extname, flag)))
            {
                free(data_blk);
                free(file_dir);
                puts("create_filedir: enlarge block error");
                return res;
            }
            free(data_blk);
            free(file_dir);
            return 0;
        }
        else
        { //块容量足够
            data_blk->size += sizeof(struct file_directory);
        }
    }
    else
    {
        //flag=0
        offset = 0;
        file_dir = (struct file_directory *)data_blk->data;

        while (offset < pos)
            file_dir++;
    }
    //给新建的file_directory赋值
    strcpy(file_dir->fname, filename);
    if (flag == 1 && extname[0] != '\0')
        strcpy(file_dir->fext, extname);
    file_dir->fsize = 0;
    file_dir->flag = flag;
    //找到空闲块作为起始块

    //为新建的文件申请一个空闲块
    if ((res = get_empty_blocks(1, &tmp)) == 1)
        file_dir->nStartBlock = tmp;
    else
    {
        puts("create_filedir: can not get empty blocks");
        free(data_blk);
        free(file_dir);
        return -errno;
    }
    //将要创建的文件或目录信息写入上层目录中
    write_block(par_dir_blk, data_blk);
    data_blk->size = 0;
    data_blk->nNextBlock = -1;
    strcpy(data_blk->data, "\0");

    //文件起始块内容为空
    write_block(file_dir->nStartBlock, data_blk);

    free(data_blk);
    puts("create_filedir: end");
    return 0;
}

int remove_filedir(const char *path, int flag)
{
    puts("invoke: remove_filedir");
    struct file_directory fdir;
    //读取文件属性
    if (get_filedir(path, &fdir) == -1)
    {
        puts("remove_filedir: get_filedir failed");
        return -ENOENT;
    }
    //flag与指定的不一致，则返回相应错误信息
    if (flag == 1 && fdir.flag == 2)
    {
        puts("remove_filedir: inconsistent flag");
        return -EISDIR;
    }
    else if (flag == 2 && fdir.flag == 1)
    {
        puts("remove_filedir: inconsistent flag");
        return -ENOTDIR;
    }
    //清空该文件从起始块开始的后续块
    struct data_block *data_blk = malloc(sizeof(struct data_block));
    if (flag == 1)
    {
        long next_blk = fdir.nStartBlock;
        clear_blocks(next_blk, data_blk);
    }
    else if (!is_empty_path(path)) //只能删除空的目录，目录非空返回错误信息
    {
        free(data_blk);
        puts("remove_filedir: dir is not empty");
        return -ENOTEMPTY;
    }

    fdir.flag = 0; // 标记fdir为未使用
    if (set_fdir(path, &fdir, flag) == -1)
    {
        puts("remove_filedir: set_fdir failed");
        free(data_blk);
        return -1;
    }
    puts("remove_filedir: remove finished");
    free(data_blk);
    return 0;
}

void copy_filedir(struct file_directory *a, struct file_directory *b)
{
    strcpy(a->fname, b->fname);
    strcpy(a->fext, b->fext);
    a->fsize = b->fsize;
    a->nStartBlock = b->nStartBlock;
    a->flag = b->flag;
}

int read_block(long idx, void *block)
{
    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r");
    if (fp == NULL)
    {
        puts("read_block: open file failed");
        return -1;
    }

    fseek(fp, idx * BLOCK_SIZE, SEEK_SET);
    fread(block, sizeof(struct data_block), 1, fp);
    if (ferror(fp))
    {
        puts("read_block: read file error");
        return -1;
    }
    fclose(fp);
    return 0;
}

int write_block(long idx, struct data_block *data_blk)
{
    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "w");
    if (fp == NULL)
    {
        puts("write_block: open disk error");
        return -1;
    }
    // 移动指针到编号为idx的块的位置
    fseek(fp, idx * BLOCK_SIZE, SEEK_SET);
    fwrite(data_blk, sizeof(struct data_block), 1, fp);
    if (ferror(fp))
    {
        puts("write_block: read file error");
        return -1;
    }
    fclose(fp);
    return 0;
}

int divide_path(const char *path, char *name, char *ext, long *par_dir_stblk, int flag, int *par_size)
{
    puts("divide_path: begin");
    // 原始路径
    char *origin_path = strdup(path);
    // 文件名
    char *m;
    // 扩展名
    char *n;
    struct file_directory attr;

    m = origin_path;
    if (!m)
        return -errno;  //路径为空
    m++;                //跳过第一个'/'
    n = strchr(m, '/'); //看是否有二级路径

    // 二级目录下创建目录
    if (n != NULL && flag == 2)
    {
        puts("divide_path: can not create file in secondary path");
        return -EPERM;
    }
    // 二级目录下创建文件
    else if (n != NULL)
    {
        *n = '\0';
        // n指向文件名
        n++;
        m = n;
        if (get_filedir(origin_path, &attr) == -1)
        {
            puts("divide_path: can not find parent dir");
            return -ENOENT;
        }
    }

    // 父目录为根目录
    if (n == NULL)
    {
        puts("divide_path: create entry in root");
        if (get_filedir("/", &attr) == -1)
        {
            puts("divide_path：can not find root");
            return -ENOENT;
        }
    }

    // 处理扩展名
    if (flag == 1)
    {
        puts("divide_path: file contain ext");
        n = strchr(m, '.');
        if (n != NULL)
        {
            *n = '\0';
            n++;
        }
    }

    // 检查文件名及扩展名长度
    if (flag == 1)
    {
        if (strlen(m) > MAX_FILENAME + 1)
        {
            return -ENAMETOOLONG;
        }
        else if (strlen(m) > MAX_FILENAME)
        {
            if (*(m + MAX_FILENAME) != '~')
            {
                return -ENAMETOOLONG;
            }
        }
        else if (n != NULL) //如果有后缀名
        {
            if (strlen(n) > MAX_EXTENSION + 1)
            {
                return -ENAMETOOLONG;
            }
            else if (strlen(n) > MAX_EXTENSION)
            {
                if (*(n + MAX_EXTENSION) != '~')
                {
                    return -ENAMETOOLONG;
                }
            }
        }
    }
    // 检查目录名长度
    else if (flag == 2)
    {
        if (strlen(m) > MAX_FILENAME)
        {
            return -ENAMETOOLONG;
        }
    }

    *name = '\0';
    *ext = '\0';
    if (m != NULL)
        strcpy(name, m);
    if (n != NULL)
        strcpy(ext, n);

    // 把开始块信息赋值给par_dir_stblk
    *par_dir_stblk = attr.nStartBlock;
    // 获取父目录文件的大小
    *par_size = attr.fsize;
    free(origin_path);
    if (*par_dir_stblk == -1)
        return -ENOENT;

    return 0;
}

int exist_check(struct file_directory *file_dir, char *p, char *q, int *offset, int *pos, int size, int flag)
{
    puts("exist_check: begin");
    while (*offset < size)
    {
        if (flag == 0)
            *pos = *offset;
        // 检查文件名和后缀名
        else if (flag == 1 && file_dir->flag == 1)
        {
            if (strcmp(p, file_dir->fname) == 0)
            {
                if ((*q == '\0' && strlen(file_dir->fext) == 0) || (*q != '\0' && strcmp(q, file_dir->fext) == 0))
                {
                    puts("exist_check: file already exist");
                    return -EEXIST;
                }
            }
        }
        //如果目录名匹配
        else if (flag == 2 && file_dir->flag == 2 && strcmp(p, file_dir->fname) == 0)
        {
            puts("exist_check: dir already exist");
            return -EEXIST;
        }
        file_dir++;
        *offset += sizeof(struct file_directory);
    }
    puts("exist_check: end");
    return 0;
}

int expand_block(long par_dir_stblk,
                 struct file_directory *file_dir,
                 struct data_block *data_blk,
                 long *tmp,
                 char *filename,
                 char *extname,
                 int flag)
{
    puts("expand_block: begin");
    long blk; //用于记录父目录新申请到的空闲块的位置
    tmp = malloc(sizeof(long));
    // 查找空闲块
    if (get_empty_blocks(1, tmp) == 1)
        blk = *tmp; //获取到空闲块的位置
    else
    {
        puts("expand_block: can not find empty block");
        free(filename);
        free(extname);
        free(data_blk);
        return -errno;
    }
    free(tmp);
    //找到的话直接给上层目录增加一个块
    data_blk->nNextBlock = blk;
    //回写父目录文件的数据
    write_block(par_dir_stblk, data_blk);

    //初始化新文件块的信息
    data_blk->size = sizeof(struct file_directory);
    data_blk->nNextBlock = -1;
    file_dir = (struct file_directory *)data_blk->data;
    //设置要创建的文件或目录的名字，并为这个新建的文件（目录）找到一个新的空闲块
    strcpy(file_dir->fname, filename);
    if (flag == 1 && *extname != '\0')
        strcpy(file_dir->fext, extname);
    file_dir->fsize = 0;
    file_dir->flag = flag;
    tmp = malloc(sizeof(long));

    if (get_empty_blocks(1, tmp) == 1)
        file_dir->nStartBlock = *tmp;
    else
    {
        puts("expand_block: can not find empty block");
        return -errno;
    } //如果没找到新的空闲块，说明磁盘满了

    //此时blk仍然记录着父目录新申请到的空闲块的位置，因此我们直接把构建好的file_dir写到这个块中
    write_block(blk, data_blk);
    //初始化新文件的数据的信息，写入到刚刚为新文件申请到的空闲块中
    data_blk->size = 0;
    data_blk->nNextBlock = -1;
    strcpy(data_blk->data, "\0");
    write_block(file_dir->nStartBlock, data_blk);
    puts("expand_block: end");
    return 0;
}

int mark_block(long idx, int flag)
{
    puts("mark_block: begin");
    if (idx == -1)
    {
        puts("mark_block: block index out of range");
        return -1;
    }

    int start = idx / 8;                 // 找到idx在bitmap的第几个字节中
    uint8_t mask = 1 << (8 - (idx % 8)); //构造相应位置1的掩码

    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r+");
    if (fp == NULL)
    {
        puts("mark_block: open disk error");
        return ENOENT;
    }
    // 移动指针到bitmap块
    fseek(fp, (BLOCK_SIZE * SUPER_BLOCK_COUNT) + start, SEEK_SET);
    uint8_t ob; // 读取原始数据
    fread(&ob, sizeof(uint8_t), 1, fp);

    // 将该位置1，其他位不动
    if (flag)
    {
        ob |= mask;
    }
    //将该位置0，其他位不动
    else
    {
        ob &= ~mask;
    }

    // 移动指针到bitmap块
    fseek(fp, (BLOCK_SIZE * SUPER_BLOCK_COUNT) + start, SEEK_SET);
    fwrite(&ob, sizeof(uint8_t), 1, fp);
    fclose(fp);
    puts("mark_block: mark finished");
    return 0;
}

int is_empty_path(const char *path)
{
    puts("is_empty_path: begin");
    struct data_block data_blk;
    struct file_directory attr;
    //读取属性到attr里
    if (get_filedir(path, &attr) == -1)
    {
        puts("is_empty_path: get_filedir failed");
        return 0;
    }
    long start_blk;
    start_blk = attr.nStartBlock;
    //传入路径为文件
    if (attr.flag == 1)
    {
        puts("is_empty_path: path is file");
        return 0;
    }
    //读出块信息
    if (read_block(start_blk, &data_blk) == -1)
    {
        puts("is_empty_path: read block failed");
        return 0;
    }

    struct file_directory *file_dir = (struct file_directory *)data_blk.data;
    int pos = 0;
    //遍历目录
    while (pos < data_blk.size)
    {
        // 目录下有文件或目录
        if (file_dir->flag != 0)
        {
            puts("is_empty_path: path is not empty");
            return 0;
        }
        file_dir++;
        pos += sizeof(struct file_directory);
    }
    puts("is_empty_path: end");
    return 1;
}

int set_fdir(const char *path, struct file_directory *fdir, int flag)
{
    puts("set_fdir: begin");
    int res, par_size;
    struct data_block block;
    // 文件名
    char filename[MAX_FILENAME + 1];
    char extname[MAX_EXTENSION + 1];
    long start_blk;

    //path所指文件的file_directory存储在该文件所在的父目录的文件块里面，所以要进行路径分割
    if ((res = divide_path(path, filename, extname, &start_blk, flag, &par_size)))
    {
        puts("set_fdir: divide_path error");
        return res;
    }

    // 寻找文件块的位置
    //获取到父目录文件的起始块位置，我们就读出这个起始块
    if (read_block(start_blk, &block) == -1)
    {
        puts("set_fdir: read block error");
        return -1;
    }
    struct file_directory *file_dir = (struct file_directory *)block.data;
    int offset = 0;
    while (offset < block.size)
    {
        //循环目录块内容
        if (file_dir->flag != 0 && strcmp(filename, file_dir->fname) == 0 && (extname[0] == '\0' || strcmp(extname, file_dir->fext) == 0)) // 允许不包含扩展名
        {
            puts("set_fdir: find file_directory");
            //设置为fdir指定的属性
            copy_filedir(file_dir, fdir);
            res = 0;
            //将修改后的目录信息写回磁盘文件
            if (write_block(start_blk, &block) == -1)
            {
                puts("set_fdir: write block error");
                res = -1;
            }
            return res;
        }
        //读下一个文件
        file_dir++;
        offset += sizeof(struct file_directory);
    }
    puts("set_filedir: can not find file");
    return -1;
}

void clear_blocks(long idx, struct data_block *block)
{
    puts("clear_blocks: start");
    while (idx != -1)
    {
        mark_block(idx, 0);     // 标记相应位为未使用
        read_block(idx, block); // 先读出当前块
        idx = block->nNextBlock;
    }
    puts("clear_blocks: clear finished");
}

int find_off_blk(long *start_blk, long *offset, struct data_block *data_blk)
{
    puts("find_off_blk: begin");
    while (1)
    {
        if (read_block(*start_blk, data_blk) == -1)
        {
            puts("find_off_blk: read block failed");
            return -errno;
        }

        //offset在当前块中，说明当前文件没有跨多个块，只要在当前块找offset即可
        if (*offset <= data_blk->size)
            break;

        //否则要在后续块中找offset，先减去当前块容量（当前文件跨了很多个块）
        *offset -= data_blk->size;
        *start_blk = data_blk->nNextBlock;
    }
    puts("find_off_blk: end");
    return 0;
}

int get_empty_blocks(int num, long *start_blk)
{
    puts("get_empty_blocks: begin");
    //从头开始找，跳过超级块、bitmap块和根目录块
    *start_blk = SUPER_BLOCK_COUNT + BITMAP_BLOCK_COUNT + ROOT_BLOCK_COUNT;
    //打开文件
    FILE *fp = NULL;
    fp = fopen(DISK_PATH, "r");
    if (fp == NULL)
    {
        puts("get_empty_blocks: open disk failed");
        return ENOENT;
    }
    // 起始字节
    int start;
    // 起始位
    uint8_t mask;
    uint8_t f;
    uint8_t flag;

    // 找到的可用连续块数目
    int tmp = 0;
    // 找到的最大连续块长度
    int max = 0;
    // 找到的最大连续块起始位置
    long max_start = -1;

    struct super_block super_block;
    fread(&super_block, sizeof(struct super_block), 1, fp);
    while (*start_blk < super_block.fs_size)
    {
        // 更新起始位置
        start = *start_blk / 8;
        mask = 1 << (8 - (*start_blk % 8));
        // 移动指针到起始字节位置
        fseek(fp, BLOCK_SIZE + start, SEEK_SET);
        fread(&flag, sizeof(uint8_t), 1, fp);
        f = flag;

        // 检查连续存储空间是否满足num块大小的要求
        for (tmp = 0; tmp < num; tmp++)
        {
            // 当前位被占用
            if ((f & mask) == mask)
                break;

            // 当前字节已结束
            if ((mask & 0x01) == 0x01)
            {
                // 读取下一字节
                fread(&flag, sizeof(uint8_t), 1, fp);
                f = flag;
                mask = 0x80; // 二进制：1000 0000
            }
            else
            {
                mask >>= 1;
            }
        }

        // 找到更大的空间
        if (tmp > max)
        {
            //记录这个连续块的起始位
            max_start = *start_blk;
            max = tmp;
        }

        // 找到了num个连续的空白块（首次适应）
        if (tmp == num)
            break;

        // 否则当前空闲块不够大
        // 更新起始块号，继续找
        *start_blk = (tmp + 1) + *start_blk;
    }
    *start_blk = max_start;
    fclose(fp);
    int j = max_start;
    int i;

    // 标记这些空闲块
    for (i = 0; i < max; i++)
    {
        if (mark_block(j++, 1) == -1)
        {
            puts("get_empty_blocks: mark block failed");
            return -1;
        }
    }
    puts("get_empty_blocks: successful");
    return max;
}
