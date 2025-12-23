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
#include <pwd.h>
#include <grp.h>

#define MAGIC_NUMBER 0x4D4F4445  // "MODE" در هگز
#define VERSION 3  // نسخه رو افزایش می‌دیم
#define BLOCK_SIZE 4096
#define MAX_FILENAME 256
#define MAX_FILES 1000
#define MAX_USERS 100
#define MAX_GROUPS 50
#define MAX_USERNAME 32
#define MAX_GROUPNAME 32
#define MAX_FREE_BLOCKS 100
#define FS_SIZE (100 * 1024 * 1024) // 100MB

// ساختار سوپر بلاک
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t last_used_byte;
    uint32_t file_count;
    uint32_t user_count;
    uint32_t group_count;
    uint32_t free_block_count;
    uint8_t padding[BLOCK_SIZE - 28];
} superblock_t;

// ساختار کاربر
typedef struct {
    char username[MAX_USERNAME];
    uint32_t uid;           // User ID
    uint32_t gid;           // Primary Group ID
    uint32_t gids[10];      // Supplementary Group IDs
    uint8_t gid_count;      // تعداد گروه‌های اضافی
    uint8_t is_root;        // آیا کاربر root است؟
    uint8_t padding[BLOCK_SIZE - (MAX_USERNAME + 48)];
} user_entry_t;

// ساختار گروه
typedef struct {
    char groupname[MAX_GROUPNAME];
    uint32_t gid;           // Group ID
    uint32_t members[50];   // اعضای گروه (UIDها)
    uint8_t member_count;   // تعداد اعضا
    uint8_t padding[BLOCK_SIZE - (MAX_GROUPNAME + 205)];
} group_entry_t;

// ساختار entry فایل
typedef struct {
    char name[MAX_FILENAME];
    uint32_t type;          // 0: file, 1: directory
    uint32_t permissions;   // مجوزهای دسترسی
    uint32_t size;
    uint32_t data_offset;
    uint32_t data_blocks;
    uint32_t uid;           // User ID مالک
    uint32_t gid;           // Group ID مالک
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint8_t padding[BLOCK_SIZE - (MAX_FILENAME + 44)];
} file_entry_t;

// ساختار ACL برای دسترسی‌های پیشرفته
typedef struct acl_entry {
    uint32_t uid_or_gid;    // UID یا GID
    uint8_t is_group;       // 0 = user, 1 = group
    uint16_t permissions;   // مجوزهای خاص
    struct acl_entry *next;
} acl_entry_t;

// ساختار بلوک خالی در لیست پیوندی
typedef struct free_block {
    uint32_t start_block;
    uint32_t block_count;
    struct free_block *next;
} free_block_t;

// ساختار state برای FUSE
struct fs_state {
    char *disk_file;
    int fd;
    void *data;
    superblock_t *superblock;
    file_entry_t *file_table;
    user_entry_t *user_table;
    group_entry_t *group_table;
    free_block_t *free_list;
    acl_entry_t **file_acls;  // لیست ACL برای هر فایل
};

// توابع مدیریت دیسک
int fs_disk_init(const char *disk_file, struct fs_state *state);
int fs_disk_open(const char *disk_file, struct fs_state *state);
void fs_disk_close(struct fs_state *state);

// توابع مدیریت فایل
file_entry_t *fs_find_file(const char *path, struct fs_state *state);
int fs_create_file(const char *path, mode_t mode, uint32_t type, struct fs_state *state);
int fs_resize_file(file_entry_t *entry, uint32_t new_size, struct fs_state *state);

// توابع مدیریت کاربران و گروه‌ها
int fs_add_user(const char *username, uint32_t uid, uint32_t gid, struct fs_state *state);
int fs_delete_user(const char *username, struct fs_state *state);
int fs_add_group(const char *groupname, uint32_t gid, struct fs_state *state);
int fs_delete_group(const char *groupname, struct fs_state *state);
int fs_add_user_to_group(const char *username, const char *groupname, struct fs_state *state);
user_entry_t *fs_find_user(const char *username, struct fs_state *state);
user_entry_t *fs_find_user_by_uid(uint32_t uid, struct fs_state *state);
group_entry_t *fs_find_group(const char *groupname, struct fs_state *state);
group_entry_t *fs_find_group_by_gid(uint32_t gid, struct fs_state *state);
int fs_check_permission(file_entry_t *file, uint32_t uid, uint32_t gid, uint32_t required_perms);

// توابع مدیریت دسترسی‌ها
int fs_chmod(const char *path, mode_t mode, struct fs_state *state);
int fs_chown(const char *path, uint32_t uid, uint32_t gid, struct fs_state *state);
int fs_chgrp(const char *path, uint32_t gid, struct fs_state *state);
void fs_print_acl(const char *path, struct fs_state *state);

// توابع مدیریت بلوک‌های خالی
int fs_alloc_blocks(uint32_t block_count, struct fs_state *state, uint32_t *start_block);
int fs_free_blocks(uint32_t start_block, uint32_t block_count, struct fs_state *state);
void fs_print_free_list(struct fs_state *state);
void fs_visualize_free_space(struct fs_state *state);

// توابع کمکی
struct fs_state *get_fs_state(void);
void fs_init_free_list(struct fs_state *state);

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
int fs_access(const char *path, int mask);

#endif