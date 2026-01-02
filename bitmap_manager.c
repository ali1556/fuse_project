#include "general_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// مقداردهی اولیه bitmap
void fs_init_bitmap(struct fs_state *state) {
    if (!state || !state->bitmap) return;
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    uint32_t bitmap_size = BITMAP_SIZE;
    
    // صفر کردن کل bitmap (همه بلوک‌ها آزاد)
    memset(state->bitmap, 0, bitmap_size);
    
    // علامت‌گذاری بلوک‌های استفاده شده
    // بلوک 0: سوپر بلاک و metadata
    bitmap_set_bit(state->bitmap, METADATA_BLOCK);
    
    // بلوک‌های bitmap خودش
    for (uint32_t i = BITMAP_BLOCK; i < BITMAP_BLOCK + BITMAP_BLOCKS; i++) {
        if (i < total_blocks) {
            bitmap_set_bit(state->bitmap, i);
        }
    }
    
    // بلوک‌های جداول سیستم
    uint32_t metadata_end = state->superblock->last_used_byte / BLOCK_SIZE;
    for (uint32_t i = 0; i < metadata_end; i++) {
        if (i < total_blocks) {
            bitmap_set_bit(state->bitmap, i);
        }
    }
    
    // بلوک‌های فایل‌های موجود (در حالت open)
    for (uint32_t i = 0; i < state->superblock->file_count; i++) {
        file_entry_t *file = &state->file_table[i];
        uint32_t start_block = file->data_offset / BLOCK_SIZE;
        for (uint32_t j = 0; j < file->data_blocks; j++) {
            if (start_block + j < total_blocks) {
                bitmap_set_bit(state->bitmap, start_block + j);
            }
        }
    }
    
    // محاسبه تعداد بلوک‌های آزاد
    state->superblock->free_blocks_count = 0;
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!bitmap_test_bit(state->bitmap, i)) {
            state->superblock->free_blocks_count++;
        }
    }
    
    printf("Bitmap initialized: %u total blocks, %u free blocks\n", 
           total_blocks, state->superblock->free_blocks_count);
}

// پیدا کردن دنباله‌ای از بلوک‌های خالی
int bitmap_find_free_blocks(uint8_t *bitmap, uint32_t total_blocks, 
                           uint32_t needed_blocks, uint32_t *start_block) {
    if (!bitmap || needed_blocks == 0 || !start_block) return -1;
    
    uint32_t consecutive_free = 0;
    
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!bitmap_test_bit(bitmap, i)) {
            if (consecutive_free == 0) {
                *start_block = i;
            }
            consecutive_free++;
            if (consecutive_free == needed_blocks) {
                return 0;
            }
        } else {
            consecutive_free = 0;
        }
    }
    
    return -1;
}

// تخصیص بلوک با استفاده از bitmap
int fs_alloc_blocks(uint32_t block_count, struct fs_state *state, uint32_t *start_block) {
    #ifdef TRACY_ENABLE
    TracyCZoneN(ctx, "fs_alloc_blocks", true);
    #endif
    
    if (!state || !state->bitmap || block_count == 0 || !start_block) {
        #ifdef TRACY_ENABLE
        TracyCZoneEnd(ctx);
        #endif
        return -EINVAL;
    }
    
    #ifdef TRACY_ENABLE
    TracyCPlot("alloc_request_size", block_count);
    #endif
    
    if (block_count > state->superblock->free_blocks_count) {
        printf("Error: Not enough free blocks (needed: %u, available: %u)\n",
               block_count, state->superblock->free_blocks_count);
        #ifdef TRACY_ENABLE
        TracyCZoneEnd(ctx);
        #endif
        return -ENOSPC;
    }
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    uint32_t found_start = 0;
    
    #ifdef TRACY_ENABLE
    TracyCZoneText(ctx, "Finding free blocks", 18);
    #endif
    
    // ابتدا سعی می‌کنیم بلوک‌های مجاور پیدا کنیم
    if (bitmap_find_free_blocks(state->bitmap, total_blocks, block_count, &found_start) == 0) {
        *start_block = found_start;
    } else {
        // اگر بلوک‌های مجاور پیدا نشد، از first-fit استفاده می‌کنیم
        #ifdef TRACY_ENABLE
        TracyCZoneText(ctx, "Using first-fit", 15);
        #endif
        uint32_t allocated = 0;
        for (uint32_t i = 0; i < total_blocks && allocated < block_count; i++) {
            if (!bitmap_test_bit(state->bitmap, i)) {
                if (allocated == 0) {
                    *start_block = i;
                }
                allocated++;
            }
        }
        
        if (allocated < block_count) {
            #ifdef TRACY_ENABLE
            TracyCZoneEnd(ctx);
            #endif
            return -ENOSPC;
        }
    }
    
    #ifdef TRACY_ENABLE
    TracyCZoneText(ctx, "Marking blocks as used", 22);
    #endif
    
    // علامت‌گذاری بلوک‌های اختصاص داده شده
    for (uint32_t i = 0; i < block_count; i++) {
        uint32_t block = *start_block + i;
        if (block < total_blocks) {
            bitmap_set_bit(state->bitmap, block);
        }
    }
    
    state->superblock->free_blocks_count -= block_count;
    
    #ifdef TRACY_ENABLE
    TracyCPlot("free_blocks_remaining", state->superblock->free_blocks_count);
    #endif
    
    printf("Allocated %u blocks starting at block %u\n", block_count, *start_block);
    
    #ifdef TRACY_ENABLE
    TracyCZoneEnd(ctx);
    #endif
    return 0;
}

// آزادسازی بلوک با استفاده از bitmap
int fs_free_blocks(uint32_t start_block, uint32_t block_count, struct fs_state *state) {
    #ifdef TRACY_ENABLE
    TracyCZoneN(ctx, "fs_free_blocks", true);
    #endif
    
    if (!state || !state->bitmap || block_count == 0) {
        #ifdef TRACY_ENABLE
        TracyCZoneEnd(ctx);
        #endif
        return -EINVAL;
    }
    
    printf("Freeing %u blocks starting at block %u\n", block_count, start_block);
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    
    // پاک کردن بلوک‌ها در bitmap
    for (uint32_t i = 0; i < block_count; i++) {
        uint32_t block = start_block + i;
        if (block < total_blocks) {
            bitmap_clear_bit(state->bitmap, block);
        }
    }
    
    state->superblock->free_blocks_count += block_count;
    
    #ifdef TRACY_ENABLE
    TracyCPlot("free_blocks_after_free", state->superblock->free_blocks_count);
    TracyCZoneEnd(ctx);
    #endif
    return 0;
}

// نمایش اطلاعات bitmap
void fs_print_bitmap_info(struct fs_state *state) {
    if (!state || !state->bitmap) {
        printf("Bitmap not initialized\n");
        return;
    }
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    uint32_t used_blocks = 0;
    uint32_t max_free_run = 0;
    uint32_t current_free_run = 0;
    
    printf("=== Bitmap Information ===\n");
    printf("Total blocks: %u\n", total_blocks);
    printf("Free blocks: %u\n", state->superblock->free_blocks_count);
    printf("Used blocks: %u\n", total_blocks - state->superblock->free_blocks_count);
    
    // محاسبه بزرگترین دنباله خالی
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (bitmap_test_bit(state->bitmap, i)) {
            used_blocks++;
            current_free_run = 0;
        } else {
            current_free_run++;
            if (current_free_run > max_free_run) {
                max_free_run = current_free_run;
            }
        }
    }
    
    printf("Largest free block run: %u blocks\n", max_free_run);
    printf("Fragmentation level: %.2f%%\n", 
           (float)(total_blocks - state->superblock->free_blocks_count - max_free_run) * 100 / total_blocks);
    printf("==========================\n");
}

// نمایش بصری فضای خالی
void fs_visualize_free_space(struct fs_state *state) {
    if (!state) return;
    
    printf("\n=== Disk Space Visualization (Bitmap) ===\n");
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    uint32_t display_blocks = total_blocks > 200 ? 200 : total_blocks; // نمایش حداکثر 200 بلوک
    
    // ایجاد آرایه برای نمایش وضعیت
    char *visual = malloc(display_blocks + 1);
    if (!visual) return;
    
    for (uint32_t i = 0; i < display_blocks; i++) {
        visual[i] = bitmap_test_bit(state->bitmap, i) ? '#' : '.';
    }
    visual[display_blocks] = '\0';
    
    printf("Total blocks: %u (%u MB)\n", total_blocks, FS_SIZE / (1024 * 1024));
    printf("Legend: # = Used, . = Free\n\n");
    
    // نمایش در خطوط 50 بلوکی
    for (uint32_t i = 0; i < display_blocks; i += 50) {
        printf("%5u-%-5u: ", i, i + 49 < display_blocks ? i + 49 : display_blocks - 1);
        
        uint32_t end = i + 50;
        if (end > display_blocks) end = display_blocks;
        
        for (uint32_t j = i; j < end; j++) {
            putchar(visual[j]);
            if ((j - i + 1) % 10 == 0) putchar(' ');
        }
        putchar('\n');
    }
    
    // آمار
    uint32_t free_blocks = state->superblock->free_blocks_count;
    uint32_t used_blocks = total_blocks - free_blocks;
    
    printf("\nStatistics:\n");
    printf("Used blocks:  %u (%.1f%%)\n", used_blocks, (float)used_blocks * 100 / total_blocks);
    printf("Free blocks:  %u (%.1f%%)\n", free_blocks, (float)free_blocks * 100 / total_blocks);
    printf("Total space:  %u MB\n", FS_SIZE / (1024 * 1024));
    printf("==============================\n\n");
    
    free(visual);
}

// ایجاد فایل‌های تست
int fs_stress_test_create_files(struct fs_state *state, int num_files) {
    #ifdef TRACY_ENABLE
    TracyCZoneN(ctx, "fs_stress_test_create_files", true);
    #endif
    
    char filename[256];
    
    for (int i = 0; i < num_files; i++) {
        snprintf(filename, sizeof(filename), "stress_file_%d.txt", i);
        
        // ایجاد فایل
        char path[300];
        snprintf(path, sizeof(path), "/%s", filename);
        
        int res = fs_create_file(path, 0644, 0, state);
        if (res < 0) {
            printf("Failed to create file %s (error: %d)\n", filename, res);
            #ifdef TRACY_ENABLE
            TracyCZoneEnd(ctx);
            #endif
            return -1;
        }
        
        // نوشتن داده تصادفی
        file_entry_t *file = fs_find_file(path, state);
        if (file) {
            // اندازه تصادفی بین 1 تا 100 بلوک
            uint32_t random_size = (rand() % 100 + 1) * BLOCK_SIZE;
            if (fs_resize_file(file, random_size, state) < 0) {
                printf("Failed to resize file %s\n", filename);
                #ifdef TRACY_ENABLE
                TracyCZoneEnd(ctx);
                #endif
                return -1;
            }
            
            // نوشتن داده
            char *data = malloc(random_size);
            if (data) {
                memset(data, 'A' + (i % 26), random_size);
                // نوشتن در حافظه
                char *data_ptr = (char *)state->data + file->data_offset;
                memcpy(data_ptr, data, random_size);
                free(data);
            }
        }
        
        if (i % 1000 == 0 && i > 0) {
            printf("Created %d files...\n", i);
            #ifdef TRACY_ENABLE
            TracyCFrameMark;
            #endif
        }
    }
    
    printf("Created %d files successfully\n", num_files);
    
    #ifdef TRACY_ENABLE
    TracyCZoneEnd(ctx);
    #endif
    return 0;
}

// عملیات تصادفی
int fs_stress_test_random_operations(struct fs_state *state, int num_operations) {
    #ifdef TRACY_ENABLE
    TracyCZoneN(ctx, "fs_stress_test_random_operations", true);
    #endif
    
    int operation_count = 0;
    int files_created = 0;
    int files_deleted = 0;
    int reads_performed = 0;
    int writes_performed = 0;
    int resizes_performed = 0;
    
    for (int i = 0; i < num_operations; i++) {
        int op = rand() % 5;
        
        switch (op) {
            case 0: // خواندن از فایل موجود
                if (state->superblock->file_count > 0) {
                    int file_idx = rand() % state->superblock->file_count;
                    file_entry_t *file = &state->file_table[file_idx];
                    
                    // شبیه‌سازی خواندن
                    #ifdef TRACY_ENABLE
                    TracyCZoneN(read_ctx, "simulated_read", true);
                    TracyCZoneEnd(read_ctx);
                    #endif
                    reads_performed++;
                    operation_count++;
                }
                break;
                
            case 1: // نوشتن به فایل موجود
                if (state->superblock->file_count > 0) {
                    int file_idx = rand() % state->superblock->file_count;
                    file_entry_t *file = &state->file_table[file_idx];
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneN(write_ctx, "file_resize", true);
                    #endif
                    
                    // تغییر اندازه تصادفی
                    uint32_t new_size = (rand() % 100 + 1) * BLOCK_SIZE;
                    fs_resize_file(file, new_size, state);
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneEnd(write_ctx);
                    #endif
                    
                    writes_performed++;
                    operation_count++;
                }
                break;
                
            case 2: // ایجاد فایل جدید
                if (state->superblock->file_count < MAX_FILES) {
                    char filename[256];
                    snprintf(filename, sizeof(filename), "temp_file_%d_%d.txt", i, rand());
                    
                    char path[300];
                    snprintf(path, sizeof(path), "/%s", filename);
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneN(create_ctx, "create_file_op", true);
                    #endif
                    
                    fs_create_file(path, 0644, 0, state);
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneEnd(create_ctx);
                    #endif
                    
                    files_created++;
                    operation_count++;
                }
                break;
                
            case 3: // حذف فایل
                if (state->superblock->file_count > 0) {
                    int file_idx = rand() % state->superblock->file_count;
                    char *filename = state->file_table[file_idx].name;
                    
                    char path[300];
                    snprintf(path, sizeof(path), "/%s", filename);
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneN(delete_ctx, "delete_file_op", true);
                    #endif
                    
                    fs_unlink(path);
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneEnd(delete_ctx);
                    #endif
                    
                    files_deleted++;
                    operation_count++;
                }
                break;
                
            case 4: // تغییر اندازه فایل
                if (state->superblock->file_count > 0) {
                    int file_idx = rand() % state->superblock->file_count;
                    file_entry_t *file = &state->file_table[file_idx];
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneN(resize_ctx, "resize_file_op", true);
                    #endif
                    
                    uint32_t new_size = (rand() % 200) * BLOCK_SIZE;
                    fs_resize_file(file, new_size, state);
                    
                    #ifdef TRACY_ENABLE
                    TracyCZoneEnd(resize_ctx);
                    #endif
                    
                    resizes_performed++;
                    operation_count++;
                }
                break;
        }
        
        if (i % 1000 == 0 && i > 0) {
            printf("Performed %d operations...\n", i);
            #ifdef TRACY_ENABLE
            TracyCFrameMark;
            #endif
        }
    }
    
    printf("Performed %d random operations:\n", operation_count);
    printf("  Files created: %d\n", files_created);
    printf("  Files deleted: %d\n", files_deleted);
    printf("  Reads: %d\n", reads_performed);
    printf("  Writes: %d\n", writes_performed);
    printf("  Resizes: %d\n", resizes_performed);
    
    #ifdef TRACY_ENABLE
    TracyCPlot("files_created", files_created);
    TracyCPlot("files_deleted", files_deleted);
    TracyCPlot("reads_performed", reads_performed);
    TracyCPlot("writes_performed", writes_performed);
    TracyCPlot("resizes_performed", resizes_performed);
    TracyCZoneEnd(ctx);
    #endif
    return 0;
}

// تست استرس
void stress_test(struct fs_state *state) {
    #ifdef TRACY_ENABLE
    TracyCZoneN(ctx, "stress_test", true);
    #endif
    
    if (!state) {
        printf("Error: Filesystem not initialized\n");
        #ifdef TRACY_ENABLE
        TracyCZoneEnd(ctx);
        #endif
        return;
    }
    
    printf("\n=== Starting Stress Test ===\n");
    printf("Creating 10000 files...\n");
    
    #ifdef TRACY_ENABLE
    TracyCPlot("stress_test_started", 1);
    #endif
    
    clock_t start = clock();
    
    // استفاده از seed ثابت برای نتایج قابل تکرار
    srand(42);
    
    #ifdef TRACY_ENABLE
    TracyCZoneText(ctx, "Creating files phase", 20);
    #endif
    
    // ایجاد فایل‌ها
    if (fs_stress_test_create_files(state, 10000) < 0) {
        printf("Stress test failed at file creation\n");
        #ifdef TRACY_ENABLE
        TracyCZoneEnd(ctx);
        #endif
        return;
    }
    
    clock_t file_creation_end = clock();
    double file_creation_time = (double)(file_creation_end - start) / CLOCKS_PER_SEC;
    printf("File creation time: %.2f seconds\n", file_creation_time);
    
    #ifdef TRACY_ENABLE
    TracyCPlot("file_creation_time", file_creation_time);
    #endif
    
    printf("Performing 10000 random operations...\n");
    
    #ifdef TRACY_ENABLE
    TracyCZoneText(ctx, "Random operations phase", 23);
    #endif
    
    // انجام عملیات تصادفی
    if (fs_stress_test_random_operations(state, 10000) < 0) {
        printf("Stress test failed at random operations\n");
        #ifdef TRACY_ENABLE
        TracyCZoneEnd(ctx);
        #endif
        return;
    }
    
    clock_t end = clock();
    double total_time = (double)(end - start) / CLOCKS_PER_SEC;
    
    #ifdef TRACY_ENABLE
    TracyCPlot("total_stress_test_time", total_time);
    #endif
    
    printf("\n=== Stress Test Results ===\n");
    printf("Total time: %.2f seconds\n", total_time);
    printf("Operations per second: %.2f\n", 20000.0 / total_time);
    printf("Files in system: %u\n", state->superblock->file_count);
    printf("Free blocks: %u\n", state->superblock->free_blocks_count);
    printf("Fragmentation: ");
    fs_print_bitmap_info(state);
    printf("=============================\n");
    
    #ifdef TRACY_ENABLE
    TracyCZoneEnd(ctx);
    #endif
}

// تابع کمکی برای بررسی یکپارچگی bitmap
int fs_check_bitmap_integrity(struct fs_state *state) {
    if (!state || !state->bitmap) return -1;
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    uint32_t calculated_free = 0;
    
    // محاسبه بلوک‌های آزاد از bitmap
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!bitmap_test_bit(state->bitmap, i)) {
            calculated_free++;
        }
    }
    
    // مقایسه با مقدار ذخیره شده
    if (calculated_free != state->superblock->free_blocks_count) {
        printf("Bitmap integrity error: calculated %u free blocks, but superblock says %u\n",
               calculated_free, state->superblock->free_blocks_count);
        return -1;
    }
    
    printf("Bitmap integrity check passed: %u free blocks\n", calculated_free);
    return 0;
}

// تابع کمکی برای پیدا کردن فرگمنت‌ها
void fs_analyze_fragmentation(struct fs_state *state) {
    if (!state || !state->bitmap) return;
    
    uint32_t total_blocks = TOTAL_BLOCKS;
    uint32_t free_runs = 0;
    uint32_t current_run = 0;
    uint32_t total_free = 0;
    
    printf("=== Fragmentation Analysis ===\n");
    
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!bitmap_test_bit(state->bitmap, i)) {
            total_free++;
            if (current_run == 0) {
                free_runs++;
            }
            current_run++;
        } else {
            if (current_run > 0) {
                printf("  Free run %u: %u blocks\n", free_runs, current_run);
                current_run = 0;
            }
        }
    }
    
    // آخرین run
    if (current_run > 0) {
        printf("  Free run %u: %u blocks\n", free_runs, current_run);
    }
    
    printf("Total free blocks: %u\n", total_free);
    printf("Number of free runs: %u\n", free_runs);
    printf("Average run size: %.2f blocks\n", 
           free_runs > 0 ? (float)total_free / free_runs : 0.0);
    printf("Fragmentation ratio: %.2f%%\n",
           free_runs > 1 ? (float)(free_runs - 1) * 100 / total_free : 0.0);
    printf("==============================\n");
}