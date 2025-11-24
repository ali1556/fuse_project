#include "general_fs.h"
#include <signal.h>
#include <execinfo.h>

void signal_handler(int sig) {
    void *array[10];
    size_t size;

    printf("\n=== Segmentation Fault Occurred ===\n");
    
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
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
    
    state->superblock->magic = MAGIC_NUMBER;
    state->superblock->version = VERSION;
    state->superblock->last_used_byte = sizeof(superblock_t) + (sizeof(file_entry_t) * MAX_FILES);
    state->superblock->file_count = 0;
    state->superblock->free_blocks = (FS_SIZE - state->superblock->last_used_byte) / BLOCK_SIZE;
    
    state->file_table = (file_entry_t *)((char *)state->data + sizeof(superblock_t));
    printf("DEBUG: File table at %p\n", state->file_table);
    
    memset(state->file_table, 0, sizeof(file_entry_t) * MAX_FILES);
    
    printf("General FS initialized successfully\n");
    printf("Total space: %d MB, Free blocks: %u\n", FS_SIZE / (1024 * 1024), state->superblock->free_blocks);
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
    
    state->file_table = (file_entry_t *)((char *)state->data + sizeof(superblock_t));
    printf("DEBUG: File table at %p\n", state->file_table);
    
    printf("General FS mounted successfully\n");
    printf("Files: %u, Free blocks: %u\n", state->superblock->file_count, state->superblock->free_blocks);
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
        return 1;
    }
    
    printf("DEBUG: Arguments: disk_file=%s, mount_point=%s\n", argv[1], argv[2]);
    
    struct fs_state *state = calloc(1, sizeof(struct fs_state));
    if (!state) {
        perror("Failed to allocate state");
        return 1;
    }
    printf("DEBUG: State allocated at %p\n", state);
    
    state->disk_file = argv[1];
    
    if (access(state->disk_file, F_OK) == 0) {
        printf("DEBUG: Disk file exists, opening...\n");
        if (fs_disk_open(state->disk_file, state) != 0) {
            free(state);
            return 1;
        }
    } else {
        printf("DEBUG: Disk file doesn't exist, creating...\n");
        if (fs_disk_init(state->disk_file, state) != 0) {
            free(state);
            return 1;
        }
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
    printf("Starting General FUSE filesystem...\n");
    printf("Disk file: %s\n", state->disk_file);
    printf("Mount point: %s\n", argv[2]);
    
    int ret = fuse_main(fuse_argc, fuse_argv, &fs_oper, state);
    
    printf("DEBUG: FUSE main returned: %d\n", ret);
    fs_disk_close(state);
    free(state);
    
    return ret;
}