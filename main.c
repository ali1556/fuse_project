#include "general_fs.h"

// عملیات‌های FUSE
static struct fuse_operations fs_oper = {
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .open       = fs_open,
    .read       = fs_read,
    .write      = fs_write,
    .create     = fs_create,
    .unlink     = fs_unlink,
    .truncate   = fs_truncate,
    .utimens    = fs_utimens,
    .mkdir      = fs_mkdir,
    .rmdir      = fs_rmdir,
};

// مقداردهی اولیه دیسک
int fs_disk_init(const char *disk_file) {
    struct fs_state *state = FS_DATA;
    
    // ایجاد فایل دیسک
    state->fd = open(disk_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (state->fd == -1) {
        perror("Failed to create disk file");
        return -1;
    }
    
    // تنظیم سایز فایل
    if (ftruncate(state->fd, FS_SIZE) == -1) {
        perror("Failed to set disk size");
        close(state->fd);
        return -1;
    }
    
    // مپ کردن فایل
    state->data = mmap(NULL, FS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
    if (state->data == MAP_FAILED) {
        perror("Failed to mmap disk file");
        close(state->fd);
        return -1;
    }
    
    // مقداردهی اولیه سوپر بلاک
    state->superblock = (superblock_t *)state->data;
    state->superblock->magic = MAGIC_NUMBER;
    state->superblock->version = VERSION;
    state->superblock->last_used_byte = sizeof(superblock_t) + (sizeof(file_entry_t) * MAX_FILES);
    state->superblock->file_count = 0;
    state->superblock->free_blocks = (FS_SIZE - state->superblock->last_used_byte) / BLOCK_SIZE;
    
    // مقداردهی اولیه جدول فایل‌ها
    state->file_table = (file_entry_t *)((char *)state->data + sizeof(superblock_t));
    memset(state->file_table, 0, sizeof(file_entry_t) * MAX_FILES);
    
    printf("General FS initialized successfully\n");
    printf("Total space: %d MB, Free blocks: %u\n", FS_SIZE / (1024 * 1024), state->superblock->free_blocks);
    return 0;
}

// باز کردن دیسک موجود
int fs_disk_open(const char *disk_file) {
    struct fs_state *state = FS_DATA;
    
    state->fd = open(disk_file, O_RDWR);
    if (state->fd == -1) {
        perror("Failed to open disk file");
        return -1;
    }
    
    state->data = mmap(NULL, FS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
    if (state->data == MAP_FAILED) {
        perror("Failed to mmap disk file");
        close(state->fd);
        return -1;
    }
    
    state->superblock = (superblock_t *)state->data;
    state->file_table = (file_entry_t *)((char *)state->data + sizeof(superblock_t));
    
    // بررسی magic number
    if (state->superblock->magic != MAGIC_NUMBER) {
        fprintf(stderr, "Invalid magic number: 0x%08X\n", state->superblock->magic);
        munmap(state->data, FS_SIZE);
        close(state->fd);
        return -1;
    }
    
    printf("General FS mounted successfully\n");
    printf("Files: %u, Free blocks: %u\n", state->superblock->file_count, state->superblock->free_blocks);
    return 0;
}

void fs_disk_close(void) {
    struct fs_state *state = FS_DATA;
    
    if (state->data != NULL) {
        munmap(state->data, FS_SIZE);
    }
    if (state->fd != -1) {
        close(state->fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <disk_file> <mount_point> [FUSE options]\n", argv[0]);
        fprintf(stderr, "Example: %s my_disk.bin /mnt/my_fs -f\n", argv[0]);
        return 1;
    }
    
    // ایجاد state
    struct fs_state *state = calloc(1, sizeof(struct fs_state));
    if (!state) {
        perror("Failed to allocate state");
        return 1;
    }
    
    state->disk_file = argv[1];
    
    // بررسی وجود فایل دیسک
    if (access(state->disk_file, F_OK) == 0) {
        // فایل وجود دارد - mount
        if (fs_disk_open(state->disk_file) != 0) {
            free(state);
            return 1;
        }
    } else {
        // فایل وجود ندارد - ایجاد جدید
        if (fs_disk_init(state->disk_file) != 0) {
            free(state);
            return 1;
        }
    }
    
    // تنظیم آرگومان‌های FUSE
    char *fuse_argv[argc + 2];
    int fuse_argc = 0;
    
    fuse_argv[fuse_argc++] = argv[0];
    fuse_argv[fuse_argc++] = argv[2];
    
    // افزودن option‌های پیشفرض
    fuse_argv[fuse_argc++] = "-o";
    fuse_argv[fuse_argc++] = "allow_other,default_permissions";
    
    // کپی بقیه آرگومان‌ها
    for (int i = 3; i < argc; i++) {
        fuse_argv[fuse_argc++] = argv[i];
    }
    
    fuse_argv[fuse_argc] = NULL;
    
    printf("Starting General FUSE filesystem...\n");
    printf("Disk file: %s\n", state->disk_file);
    printf("Mount point: %s\n", argv[2]);
    
    int ret = fuse_main(fuse_argc, fuse_argv, &fs_oper, state);
    
    // Cleanup
    fs_disk_close();
    free(state);
    
    return ret;
}