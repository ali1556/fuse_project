#ifndef GENERAL_FS_H
#define GENERAL_FS_H

// استفاده از FUSE معمولی به جای FUSE3
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define MAGIC_NUMBER 0x4D4F4445  // "MODE" در هگز
#define VERSION 1
#define BLOCK_SIZE 4096
#define MAX_FILENAME 256
#define MAX_FILES 1000
#define FS_SIZE (100 * 1024 * 1024) // 100MB

// ساختار سوپر بلاک
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t last_used_byte;
    uint32_t file_count;
    uint32_t free_blocks;
    uint8_t padding[BLOCK_SIZE - 20];
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
    uint32_t atime;         // access time
    uint32_t mtime;         // modification time  
    uint32_t ctime;         // creation time
    uint8_t padding[BLOCK_SIZE - (MAX_FILENAME + 40)];
} file_entry_t;

// ساختار state برای FUSE
struct fs_state {
    char *disk_file;
    int fd;
    void *data;
    superblock_t *superblock;
    file_entry_t *file_table;
};

// ماکرو برای دسترسی به state
extern struct fs_state *fs_state_global;

#endif