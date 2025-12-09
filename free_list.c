#include "general_fs.h"
#include <stdio.h>

// تابع کمکی برای تبدیل آفست به شماره بلوک
static uint32_t offset_to_block(uint32_t offset) {
    return offset / BLOCK_SIZE;
}

// تابع کمکی برای تبدیل شماره بلوک به آفست
static uint32_t block_to_offset(uint32_t block) {
    return block * BLOCK_SIZE;
}

// ایجاد یک گره جدید در لیست بلوک‌های خالی
static free_block_t *create_free_block(uint32_t start_block, uint32_t block_count) {
    free_block_t *new_block = malloc(sizeof(free_block_t));
    if (!new_block) return NULL;
    
    new_block->start_block = start_block;
    new_block->block_count = block_count;
    new_block->next = NULL;
    
    return new_block;
}

// درج یک بلوک خالی در لیست به‌صورت مرتب
static int insert_free_block(free_block_t **head, free_block_t *new_block) {
    if (!new_block) return -1;
    
    // اگر لیست خالی است
    if (*head == NULL) {
        *head = new_block;
        return 0;
    }
    
    free_block_t *current = *head;
    free_block_t *prev = NULL;
    
    // پیدا کردن موقعیت مناسب برای درج (مرتب بر اساس start_block)
    while (current && current->start_block < new_block->start_block) {
        prev = current;
        current = current->next;
    }
    
    // درج در ابتدای لیست
    if (prev == NULL) {
        new_block->next = *head;
        *head = new_block;
    } else {
        // درج در میانه یا انتهای لیست
        new_block->next = current;
        prev->next = new_block;
    }
    
    return 0;
}

// ادغام بلوک‌های خالی مجاور
static void merge_free_blocks(free_block_t *head) {
    free_block_t *current = head;
    
    while (current && current->next) {
        // اگر بلوک فعلی و بعدی مجاور هم هستند
        if ((current->start_block + current->block_count) == current->next->start_block) {
            // ادغام بلوک‌ها
            current->block_count += current->next->block_count;
            
            // حذف بلوک بعدی
            free_block_t *to_delete = current->next;
            current->next = to_delete->next;
            free(to_delete);
        } else {
            current = current->next;
        }
    }
}

// تخصیص بلوک از لیست بلوک‌های خالی
int fs_alloc_blocks(uint32_t block_count, struct fs_state *state, uint32_t *start_block) {
    if (block_count == 0 || !state || !start_block) return -1;
    
    free_block_t *current = state->free_list;
    free_block_t *prev = NULL;
    
    // جستجوی اولین بلوک خالی با اندازه کافی (First Fit)
    while (current) {
        if (current->block_count >= block_count) {
            *start_block = current->start_block;
            
            // اگر بلوک دقیقاً به اندازه نیاز باشد
            if (current->block_count == block_count) {
                // حذف کامل بلوک از لیست
                if (prev) {
                    prev->next = current->next;
                } else {
                    state->free_list = current->next;
                }
                free(current);
            } else {
                // کاهش اندازه بلوک
                current->start_block += block_count;
                current->block_count -= block_count;
            }
            
            state->superblock->free_block_count--;
            printf("Allocated %u blocks starting at block %u\n", block_count, *start_block);
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    // اگر فضای خالی کافی پیدا نشد
    printf("Error: Not enough free blocks (needed: %u)\n", block_count);
    return -ENOSPC;
}

// آزادسازی بلوک و اضافه کردن به لیست بلوک‌های خالی
int fs_free_blocks(uint32_t start_block, uint32_t block_count, struct fs_state *state) {
    if (block_count == 0 || !state) return -1;
    
    printf("Freeing %u blocks starting at block %u\n", block_count, start_block);
    
    // ایجاد گره جدید برای بلوک آزاد شده
    free_block_t *freed_block = create_free_block(start_block, block_count);
    if (!freed_block) return -ENOMEM;
    
    // درج بلوک آزاد شده در لیست
    if (insert_free_block(&state->free_list, freed_block) < 0) {
        free(freed_block);
        return -1;
    }
    
    // ادغام بلوک‌های مجاور
    merge_free_blocks(state->free_list);
    
    state->superblock->free_block_count++;
    return 0;
}

// نمایش لیست بلوک‌های خالی
void fs_print_free_list(struct fs_state *state) {
    if (!state || !state->free_list) {
        printf("Free list is empty\n");
        return;
    }
    
    printf("=== Free Block List ===\n");
    printf("Total free blocks in list: %u\n", state->superblock->free_block_count);
    
    free_block_t *current = state->free_list;
    int i = 1;
    
    while (current) {
        printf("%d. Start block: %u, Block count: %u, Size: %u KB\n", 
               i++, 
               current->start_block,
               current->block_count,
               current->block_count * BLOCK_SIZE / 1024);
        current = current->next;
    }
    printf("=======================\n");
}

// نمایش بصری فضای خالی
void fs_visualize_free_space(struct fs_state *state) {
    if (!state) return;
    
    printf("\n=== Disk Space Visualization ===\n");
    
    // محاسبه کل بلوک‌ها
    uint32_t total_blocks = FS_SIZE / BLOCK_SIZE;
    
    // ایجاد آرایه برای نمایش وضعیت هر بلوک
    char *visual = malloc(total_blocks + 1);
    if (!visual) return;
    
    // مقداردهی اولیه: همه بلوک‌ها پر فرض می‌شوند
    memset(visual, '#', total_blocks);
    visual[total_blocks] = '\0';
    
    // علامت‌گذاری بلوک‌های خالی
    free_block_t *current = state->free_list;
    while (current) {
        for (uint32_t i = 0; i < current->block_count; i++) {
            if (current->start_block + i < total_blocks) {
                visual[current->start_block + i] = '.';
            }
        }
        current = current->next;
    }
    
    // نمایش وضعیت بلوک‌ها
    printf("Total blocks: %u (%u MB)\n", total_blocks, FS_SIZE / (1024 * 1024));
    printf("Legend: # = Used, . = Free\n\n");
    
    // نمایش در خطوط 50 بلوکی
    for (uint32_t i = 0; i < total_blocks; i += 50) {
        printf("%5u-%-5u: ", i, i + 49 < total_blocks ? i + 49 : total_blocks - 1);
        
        uint32_t end = i + 50;
        if (end > total_blocks) end = total_blocks;
        
        for (uint32_t j = i; j < end; j++) {
            putchar(visual[j]);
            if ((j - i + 1) % 10 == 0) putchar(' ');
        }
        putchar('\n');
    }
    
    // آمار
    uint32_t free_blocks_count = 0;
    current = state->free_list;
    while (current) {
        free_blocks_count += current->block_count;
        current = current->next;
    }
    
    uint32_t used_blocks = total_blocks - free_blocks_count;
    printf("\nStatistics:\n");
    printf("Used blocks:  %u (%.1f%%)\n", used_blocks, (float)used_blocks * 100 / total_blocks);
    printf("Free blocks:  %u (%.1f%%)\n", free_blocks_count, (float)free_blocks_count * 100 / total_blocks);
    printf("Total space:  %u MB\n", FS_SIZE / (1024 * 1024));
    printf("==============================\n\n");
    
    free(visual);
}

// مقداردهی اولیه لیست بلوک‌های خالی
void fs_init_free_list(struct fs_state *state) {
    if (!state) return;
    
    // کل فضای دیسک را به عنوان یک بلوک خالی بزرگ در نظر بگیریم
    uint32_t total_blocks = FS_SIZE / BLOCK_SIZE;
    uint32_t used_blocks = state->superblock->last_used_byte / BLOCK_SIZE;
    
    // اگر فضای استفاده شده وجود دارد
    if (used_blocks < total_blocks) {
        free_block_t *initial_free = create_free_block(used_blocks, total_blocks - used_blocks);
        if (initial_free) {
            state->free_list = initial_free;
            state->superblock->free_block_count = 1;
        }
    } else {
        state->free_list = NULL;
        state->superblock->free_block_count = 0;
    }
}