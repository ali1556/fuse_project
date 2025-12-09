#include "general_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_usage() {
    printf("General FS Command Line Interface\n");
    printf("Usage: ./cli <disk_file> <command> [args]\n");
    printf("\nCommands:\n");
    printf("  list                    - List all files\n");
    printf("  create <file>           - Create a file\n");
    printf("  mkdir <dir>             - Create a directory\n");
    printf("  delete <file>           - Delete a file\n");
    printf("  rmdir <dir>             - Remove a directory\n");
    printf("  write <file> <data>     - Write data to file\n");
    printf("  read <file>             - Read file content\n");
    printf("  viz                     - Visualize free space\n");
    printf("  info                    - Show filesystem info\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    
    const char *disk_file = argv[1];
    const char *command = argv[2];
    
    // باز کردن دیسک
    struct fs_state state;
    memset(&state, 0, sizeof(state));
    
    if (access(disk_file, F_OK) == 0) {
        if (fs_disk_open(disk_file, &state) != 0) {
            printf("Failed to open disk file\n");
            return 1;
        }
    } else {
        printf("Disk file does not exist\n");
        return 1;
    }
    
    // اجرای دستورات
    if (strcmp(command, "list") == 0) {
        printf("Files in filesystem:\n");
        for (uint32_t i = 0; i < state.superblock->file_count; i++) {
            printf("  %s [%s]\n", 
                   state.file_table[i].name,
                   state.file_table[i].type == 1 ? "DIR" : "FILE");
        }
        
    } else if (strcmp(command, "viz") == 0) {
        fs_visualize_free_space(&state);
        
    } else if (strcmp(command, "info") == 0) {
        printf("Filesystem Information:\n");
        printf("  Magic number: 0x%08X\n", state.superblock->magic);
        printf("  Version: %u\n", state.superblock->version);
        printf("  File count: %u\n", state.superblock->file_count);
        printf("  Last used byte: %u\n", state.superblock->last_used_byte);
        printf("  Free blocks in list: %u\n", state.superblock->free_block_count);
        
        // محاسبه فضای کل و آزاد
        uint32_t total_blocks = FS_SIZE / BLOCK_SIZE;
        uint32_t free_blocks = 0;
        free_block_t *current = state.free_list;
        while (current) {
            free_blocks += current->block_count;
            current = current->next;
        }
        
        printf("  Total blocks: %u\n", total_blocks);
        printf("  Used blocks: %u\n", total_blocks - free_blocks);
        printf("  Free blocks: %u\n", free_blocks);
        printf("  Used space: %.1f%%\n", 
               (float)(total_blocks - free_blocks) * 100 / total_blocks);
        
    } else {
        printf("Unknown command: %s\n", command);
        print_usage();
    }
    
    // بستن دیسک
    fs_disk_close(&state);
    
    return 0;
}