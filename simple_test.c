#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define BLOCK_SIZE 4096
#define FS_SIZE (100 * 1024 * 1024) // 100MB

// ساختار ساده برای تست
typedef struct {
    char name[256];
    size_t size;
    time_t mtime;
} test_file_t;

void create_test_disk(const char *filename) {
    printf("Creating test disk: %s\n", filename);
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Failed to create disk file");
        exit(1);
    }
    
    if (ftruncate(fd, FS_SIZE) == -1) {
        perror("Failed to set disk size");
        close(fd);
        exit(1);
    }
    
    // نوشتن signature
    char signature[] = "GENERALFS";
    write(fd, signature, sizeof(signature));
    
    close(fd);
    printf("Test disk created successfully (%d MB)\n", FS_SIZE / (1024 * 1024));
}

void run_basic_operations() {
    printf("\n=== Basic Operations Test ===\n");
    
    // ایجاد دایرکتوری تست
    system("mkdir -p /tmp/general_fs_test");
    
    // مانت فایل سیستم (در ترمینال جداگانه)
    printf("To mount filesystem, run in another terminal:\n");
    printf("  ./general_fs test_disk.bin /tmp/general_fs_test -f\n\n");
    
    printf("Then run these commands in order:\n");
    printf("1. Create file:        touch /tmp/general_fs_test/test.txt\n");
    printf("2. Write to file:      echo 'Hello World' > /tmp/general_fs_test/test.txt\n");
    printf("3. Read file:          cat /tmp/general_fs_test/test.txt\n");
    printf("4. Create directory:   mkdir /tmp/general_fs_test/mydir\n");
    printf("5. List contents:      ls -la /tmp/general_fs_test/\n");
    printf("6. Delete file:        rm /tmp/general_fs_test/test.txt\n");
    printf("7. Unmount:           fusermount3 -u /tmp/general_fs_test\n");
}

int main() {
    printf("=== General FS Tester ===\n");
    
    create_test_disk("test_disk.bin");
    run_basic_operations();
    
    return 0;
}