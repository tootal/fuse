#include "util.h"

// 按照设计要求实现所有函数

// 读取文件属性到stat
int ufs_getattr(const char *path, struct stat *stat) {
    int res = 0;
	struct file_directory *attr = malloc(sizeof(struct file_directory));

	//非根目录
	if (get_fd_to_attr(path, attr) == -1) 
	{
		free(attr);	printf("ufs_getattr：get_fd_to_attr时发生错误，函数结束返回\n\n");
        return -1;
	}

	memset(stat, 0, sizeof(struct stat));//将stat结构中成员的值全部置0
	if (attr->flag==2) {//从path判断这个文件是		一个目录	还是	一般文件
		printf("ufs_getattr：这个file_directory是一个目录\n\n");
		stat->st_mode = S_IFDIR | 0666;//设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
		//stat->st_nlink = 2;//st_nlink是连到该文件的硬连接数目,即一个文件的一个或多个文件名。说白点，所谓链接无非是把文件名和计算机文件系统使用的节点号链接起来。因此我们可以用多个文件名与同一个文件进行链接，这些文件名可以在同一目录或不同目录。
	} 
	else if (attr->flag==1) 
	{
		printf("MFS_getattr：这个file_directory是一个文件\n\n");
		stat->st_mode = S_IFREG | 0666;//该文件是	一般文件
		stat->st_size = attr->fsize;
		//stat->st_nlink = 1;
	} 
	else {printf("MFS_getattr：这个文件（目录）不存在，函数结束返回\n\n");;res = -ENOENT;}//文件不存在
	
	printf("MFS_getattr：getattr成功，函数结束返回\n\n");
	free(attr);
	return res;
}

int ufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info) {

}

int ufs_mkdir(const char *path, mode_t mode) {

}

int ufs_rmdir(const char *path) {
    
}

int ufs_mknod(const char *path, mode_t mode, dev_t dev) {

}

int ufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
    
}

int ufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
    
}

int ufs_unlink(const char *path) {
    
}

int main(int argc, char *argv[]) {
    struct fuse_operations op = {
        .getattr = ufs_getattr,
        .readdir = ufs_readdir,
        .mkdir = ufs_mkdir,
        .rmdir = ufs_rmdir,
        .mknod = ufs_mknod,
        .write = ufs_write,
        .read = ufs_read,
        .unlink = ufs_unlink
    };
    return fuse_main(argc, argv, &op, NULL);
}