#include "ufs.h"

// 初始化超级块
void init_super_block(FILE *fp) {
    struct super_block block = {
        .fs_size = BLOCK_COUNT,
        .first_blk = SUPER_BLOCK_COUNT + BITMAP_BLOCK_COUNT,
        .bitmap = BITMAP_BLOCK_COUNT
    };
    fwrite(&block, sizeof(struct super_block), 1, fp);
    puts("super block initialized");
}

// 初始化bitmap块
void init_bitmap_block(FILE *fp) {
    // 初始化置0
    fseek(fp, BLOCK_SIZE * SUPER_BLOCK_COUNT, SEEK_SET);
    uint8_t a[BITMAP_BLOCK_COUNT * BLOCK_SIZE];
    memset(a, 0, sizeof(a));
    fwrite(a, sizeof(a), 1, fp);

    // 标记已使用的块
    fseek(fp, BLOCK_SIZE * SUPER_BLOCK_COUNT, SEEK_SET);
    uint8_t b[BLOCK_USED / 8];
    memset(b, 0xff, sizeof(b));
    fwrite(b, sizeof(b), 1, fp);
    // 剩余比特位
    int r = BLOCK_USED % 8;
    uint8_t c = ((1 << r) - 1) << (8 - r);
    fwrite(&c, sizeof(c), 1, fp);

    puts("bitmap block initialized");
}

// 初始化数据块
void init_data_block(FILE *fp) {
    fseek(fp, BLOCK_SIZE * (SUPER_BLOCK_COUNT + BITMAP_BLOCK_COUNT), SEEK_SET);
    struct data_block block = {
        .size = 0,
        .nNextBlock = -1
    };
    block.data[0] = '\0';
    fwrite(&block, sizeof(struct data_block), 1, fp);
    puts("data block initialized");
}

int main()
{
    system("dd bs=1K count=5K if=/dev/zero of=" DISK_PATH);
    FILE *fp=NULL;
    fp = fopen(DISK_PATH, "r+");
	if (fp == NULL) {
		puts("open disk failed");
        return ENOENT;
    }

    init_super_block(fp);
    init_bitmap_block(fp);
    init_data_block(fp);
    
    fclose(fp);
    puts("disk initialize finished");
    return 0;
}
