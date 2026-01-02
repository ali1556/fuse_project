#include "general_fs.h"
#include <signal.h>
#include <execinfo.h>

// Global state variable
struct fs_state *fs_global_state = NULL;

struct fs_state *get_fs_state() {
    return fs_global_state;
}

void signal_handler(int sig) {
    void *array[10];
    size_t size;

    printf("\n=== Segmentation Fault Occurred ===\n");
    
    size = backtrace(array, 10);
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// تابع برای مقداردهی اولیه کاربران و گروه‌ها
void fs_init_users_groups(struct fs_state *state) {
    if (!state) return;
    
    // ایجاد کاربر root
    user_entry_t *root = &state->user_table[0];
    strcpy(root->username, "root");
    root->uid = 0;
    root->gid = 0;
    root->gids[0] = 0;
    root->gid_count = 1;
    root->is_root = 1;
    
    // ایجاد گروه root
    group_entry_t *root_group = &state->group_table[0];
    strcpy(root_group->groupname, "root");
    root_group->gid = 0;
    root_group->members[0] = 0;  // کاربر root
    root_group->member_count = 1;
    
    state->superblock->user_count = 1;
    state->superblock->group_count = 1;
    
    printf("Initialized users/groups: root user and group created\n");
}

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
    .access     = fs_access,
};

// مقداردهی اولیه دیسک
int fs_disk_init(const char *disk_file, struct fs_state *state) {
    printf("DEBUG: Initializing disk...\n");
    
    state->fd = open(disk_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (state->fd == -1) {
        perror("Failed to create disk file");
        return -1;
    }
    printf("DEBUG: File created with fd: %d\n", state->fd);
    
    if (ftruncate(state->fd, FS_SIZE) == -1) {
        perror("Failed to set disk size");
        close(state->fd);
        return -1;
    }
    printf("DEBUG: File truncated to %d bytes\n", FS_SIZE);
    
    state->data = mmap(NULL, FS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
    if (state->data == MAP_FAILED) {
        perror("Failed to mmap disk file");
        close(state->fd);
        return -1;
    }
    printf("DEBUG: Memory mapping successful at %p\n", state->data);
    
    state->superblock = (superblock_t *)state->data;
    printf("DEBUG: Superblock at %p\n", state->superblock);
    
    // مقداردهی اولیه سوپر بلاک
    state->superblock->magic = MAGIC_NUMBER;
    state->superblock->version = VERSION;
    
    // محاسبه آدرس جداول
    uint32_t offset = sizeof(superblock_t);
    
    state->user_table = (user_entry_t *)((char *)state->data + offset);
    offset += sizeof(user_entry_t) * MAX_USERS;
    
    state->group_table = (group_entry_t *)((char *)state->data + offset);
    offset += sizeof(group_entry_t) * MAX_GROUPS;
    
    state->file_table = (file_entry_t *)((char *)state->data + offset);
    offset += sizeof(file_entry_t) * MAX_FILES;
    
    // محاسبه بلوک bitmap
    state->superblock->bitmap_block = offset / BLOCK_SIZE;
    state->bitmap = (uint8_t *)((char *)state->data + offset);
    state->bitmap_size = BITMAP_SIZE;
    
    offset += BITMAP_SIZE;
    state->superblock->first_data_block = offset / BLOCK_SIZE;
    
    // محاسبه last_used_byte
    state->superblock->last_used_byte = offset;
    state->superblock->file_count = 0;
    state->superblock->user_count = 0;
    state->superblock->group_count = 0;
    state->superblock->free_blocks_count = 0;
    
    printf("DEBUG: Bitmap at %p (size: %u bytes)\n", state->bitmap, state->bitmap_size);
    printf("DEBUG: First data block: %u\n", state->superblock->first_data_block);
    
    // صفر کردن حافظه
    memset(state->user_table, 0, sizeof(user_entry_t) * MAX_USERS);
    memset(state->group_table, 0, sizeof(group_entry_t) * MAX_GROUPS);
    memset(state->file_table, 0, sizeof(file_entry_t) * MAX_FILES);
    
    // مقداردهی اولیه bitmap
    fs_init_bitmap(state);
    
    // مقداردهی اولیه کاربران و گروه‌ها
    fs_init_users_groups(state);
    
    printf("General FS (Bitmap) initialized successfully\n");
    fs_print_bitmap_info(state);
    return 0;
}

// باز کردن دیسک موجود
int fs_disk_open(const char *disk_file, struct fs_state *state) {
    printf("DEBUG: Opening existing disk...\n");
    
    state->fd = open(disk_file, O_RDWR);
    if (state->fd == -1) {
        perror("Failed to open disk file");
        return -1;
    }
    printf("DEBUG: File opened with fd: %d\n", state->fd);
    
    state->data = mmap(NULL, FS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
    if (state->data == MAP_FAILED) {
        perror("Failed to mmap disk file");
        close(state->fd);
        return -1;
    }
    printf("DEBUG: Memory mapping successful at %p\n", state->data);
    
    state->superblock = (superblock_t *)state->data;
    printf("DEBUG: Superblock at %p\n", state->superblock);
    
    if (state->superblock->magic != MAGIC_NUMBER) {
        fprintf(stderr, "Invalid magic number: 0x%08X\n", state->superblock->magic);
        munmap(state->data, FS_SIZE);
        close(state->fd);
        return -1;
    }
    
    if (state->superblock->version != VERSION) {
        fprintf(stderr, "Version mismatch: expected %u, got %u\n", 
                VERSION, state->superblock->version);
        munmap(state->data, FS_SIZE);
        close(state->fd);
        return -1;
    }
    
    // محاسبه آدرس جداول
    uint32_t offset = sizeof(superblock_t);
    
    state->user_table = (user_entry_t *)((char *)state->data + offset);
    offset += sizeof(user_entry_t) * MAX_USERS;
    
    state->group_table = (group_entry_t *)((char *)state->data + offset);
    offset += sizeof(group_entry_t) * MAX_GROUPS;
    
    state->file_table = (file_entry_t *)((char *)state->data + offset);
    offset += sizeof(file_entry_t) * MAX_FILES;
    
    // محاسبه آدرس bitmap
    state->bitmap = (uint8_t *)((char *)state->data + state->superblock->bitmap_block * BLOCK_SIZE);
    state->bitmap_size = BITMAP_SIZE;
    
    printf("DEBUG: Bitmap at %p (size: %u bytes)\n", state->bitmap, state->bitmap_size);
    printf("DEBUG: First data block: %u\n", state->superblock->first_data_block);
    
    printf("General FS (Bitmap) mounted successfully\n");
    printf("Files: %u, Users: %u, Groups: %u, Free blocks: %u\n", 
           state->superblock->file_count,
           state->superblock->user_count,
           state->superblock->group_count,
           state->superblock->free_blocks_count);
    fs_print_bitmap_info(state);
    return 0;
}

void fs_disk_close(struct fs_state *state) {
    printf("DEBUG: Closing disk...\n");
    
    if (state->data != NULL) {
        munmap(state->data, FS_SIZE);
        printf("DEBUG: Memory unmapped\n");
    }
    if (state->fd != -1) {
        close(state->fd);
        printf("DEBUG: File closed\n");
    }
}

int main(int argc, char *argv[]) {
    // Register signal handler for debugging
    signal(SIGSEGV, signal_handler);
    
    printf("DEBUG: Program started\n");
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <disk_file> <mount_point> [FUSE options]\n", argv[0]);
        fprintf(stderr, "Example: %s my_disk.bin /mnt/my_fs -f\n", argv[0]);
        fprintf(stderr, "\nAdditional commands after unmount:\n");
        fprintf(stderr, "  viz - visualize free space\n");
        fprintf(stderr, "  stress - run stress test\n");
        fprintf(stderr, "  useradd <username> - add new user\n");
        fprintf(stderr, "  userdel <username> - delete user\n");
        fprintf(stderr, "  groupadd <groupname> - add new group\n");
        fprintf(stderr, "  groupdel <groupname> - delete group\n");
        return 1;
    }
    
    printf("DEBUG: Arguments: disk_file=%s, mount_point=%s\n", argv[1], argv[2]);
    
    fs_global_state = calloc(1, sizeof(struct fs_state));
    if (!fs_global_state) {
        perror("Failed to allocate state");
        return 1;
    }
    printf("DEBUG: State allocated at %p\n", fs_global_state);
    
    fs_global_state->disk_file = argv[1];
    
    if (access(fs_global_state->disk_file, F_OK) == 0) {
        printf("DEBUG: Disk file exists, opening...\n");
        if (fs_disk_open(fs_global_state->disk_file, fs_global_state) != 0) {
            free(fs_global_state);
            return 1;
        }
    } else {
        printf("DEBUG: Disk file doesn't exist, creating...\n");
        if (fs_disk_init(fs_global_state->disk_file, fs_global_state) != 0) {
            free(fs_global_state);
            return 1;
        }
    }
    
    // بررسی اگر دستور stress باشد
    if (argc >= 4 && strcmp(argv[3], "stress") == 0) {
        printf("Running stress test...\n");
        stress_test(fs_global_state);
        fs_disk_close(fs_global_state);
        free(fs_global_state);
        return 0;
    }
    
    char *fuse_argv[argc + 2];
    int fuse_argc = 0;
    
    fuse_argv[fuse_argc++] = argv[0];
    fuse_argv[fuse_argc++] = argv[2];
    fuse_argv[fuse_argc++] = "-o";
    fuse_argv[fuse_argc++] = "allow_other,default_permissions";
    
    for (int i = 3; i < argc; i++) {
        fuse_argv[fuse_argc++] = argv[i];
    }
    
    fuse_argv[fuse_argc] = NULL;
    
    printf("DEBUG: Starting FUSE main...\n");
    printf("Starting General FUSE filesystem (Bitmap Version)...\n");
    printf("Disk file: %s\n", fs_global_state->disk_file);
    printf("Mount point: %s\n", argv[2]);
    printf("Version: %u with bitmap freelist\n", VERSION);
    
    int ret = fuse_main(fuse_argc, fuse_argv, &fs_oper, NULL);
    
    printf("DEBUG: FUSE main returned: %d\n", ret);
    
    // پس از جدا کردن فایل سیستم، فضای خالی را نمایش می‌دهیم
    if (fs_global_state) {
        printf("\n=== Final Disk State ===\n");
        fs_visualize_free_space(fs_global_state);
        fs_disk_close(fs_global_state);
        free(fs_global_state);
    }
    
    return ret;
}