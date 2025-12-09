#ifndef GENERAL_FS_H
#define GENERAL_FS_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>

#define MAGIC_NUMBER 0x4D4F4445  // "MODE" در هگز
#define VERSION 2  // نسخه را افزایش می‌دهیم
#define BLOCK_SIZE 4096
#define MAX_FILENAME 256
#define MAX_FILES 1000
#define MAX_FREE_BLOCKS 100  // حداکثر تعداد بلوک‌های خالی در freelist
#define FS_SIZE (100 * 1024 * 1024) // 100MB

// ساختار سوپر بلاک
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t last_used_byte;
    uint32_t file_count;
    uint32_t free_block_count;  // تعداد بلوک‌های خالی در freelist
    uint8_t padding[BLOCK_SIZE - 24];
} superblock_t;

// ساختار entry فایل
typedef struct {
    char name[MAX_FILENAME];
    uint32_t type;          // 0: file, 1: directory
    uint32_t permissions;
    uint32_t size;
    uint32_t data_offset;
    uint32_t data_blocks;
    uint32_t uid;
    uint32_t gid;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint8_t padding[BLOCK_SIZE - (MAX_FILENAME + 40)];
} file_entry_t;

// ساختار بلوک خالی در لیست پیوندی
typedef struct free_block {
    uint32_t start_block;    // شماره بلوک شروع
    uint32_t block_count;    // تعداد بلوک‌های متوالی خالی
    struct free_block *next; // اشاره‌گر به بلوک خالی بعدی
} free_block_t;

// ساختار state برای FUSE
struct fs_state {
    char *disk_file;
    int fd;
    void *data;
    superblock_t *superblock;
    file_entry_t *file_table;
    free_block_t *free_list;  // لیست پیوندی بلوک‌های خالی
};

// توابع مدیریت دیسک
int fs_disk_init(const char *disk_file, struct fs_state *state);
int fs_disk_open(const char *disk_file, struct fs_state *state);
void fs_disk_close(struct fs_state *state);

// توابع مدیریت فایل
file_entry_t *fs_find_file(const char *path, struct fs_state *state);
int fs_create_file(const char *path, mode_t mode, uint32_t type, struct fs_state *state);
int fs_resize_file(file_entry_t *entry, uint32_t new_size, struct fs_state *state);

// توابع مدیریت بلوک‌های خالی
int fs_alloc_blocks(uint32_t block_count, struct fs_state *state, uint32_t *start_block);
int fs_free_blocks(uint32_t start_block, uint32_t block_count, struct fs_state *state);
void fs_print_free_list(struct fs_state *state);
void fs_visualize_free_space(struct fs_state *state);

// توابع FUSE
int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int fs_open(const char *path, struct fuse_file_info *fi);
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int fs_unlink(const char *path);
int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi);
int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
int fs_mkdir(const char *path, mode_t mode);
int fs_rmdir(const char *path);

#endif