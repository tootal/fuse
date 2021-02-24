#include "ufs.h"

// 按照设计要求实现所有函数

int ufs_getattr(const char *path, struct stat *stat, struct fuse_file_info *info) {

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

int ufs_read(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
    
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