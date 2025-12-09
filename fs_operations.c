#include "general_fs.h"

// پیدا کردن فایل بر اساس مسیر
file_entry_t *fs_find_file(const char *path, struct fs_state *state) {
    if (strcmp(path, "/") == 0) {
        static file_entry_t root_dir;
        memset(&root_dir, 0, sizeof(root_dir));
        strcpy(root_dir.name, "/");
        root_dir.type = 1;
        root_dir.permissions = 0755;
        root_dir.size = BLOCK_SIZE;
        root_dir.uid = getuid();
        root_dir.gid = getgid();
        root_dir.atime = root_dir.mtime = root_dir.ctime = time(NULL);
        return &root_dir;
    }

    const char *filename = path + 1;
    
    for (uint32_t i = 0; i < state->superblock->file_count; i++) {
        if (strcmp(state->file_table[i].name, filename) == 0) {
            return &state->file_table[i];
        }
    }
    
    return NULL;
}

// ایجاد فایل جدید
int fs_create_file(const char *path, mode_t mode, uint32_t type, struct fs_state *state) {
    if (state->superblock->file_count >= MAX_FILES) {
        return -ENOSPC;
    }

    const char *filename = path + 1;
    
    if (fs_find_file(path, state) != NULL) {
        return -EEXIST;
    }

    file_entry_t *entry = &state->file_table[state->superblock->file_count];
    
    strncpy(entry->name, filename, MAX_FILENAME - 1);
    entry->type = type;
    entry->permissions = mode;
    entry->size = 0;
    entry->uid = getuid();
    entry->gid = getgid();
    entry->atime = entry->mtime = entry->ctime = time(NULL);
    
    // اگر فایل معمولی است، فضایی برای آن اختصاص می‌دهیم
    if (type == 0) {
        // فایل‌های معمولی حداقل یک بلوک نیاز دارند
        uint32_t start_block;
        if (fs_alloc_blocks(1, state, &start_block) < 0) {
            return -ENOSPC;
        }
        entry->data_offset = start_block * BLOCK_SIZE;
        entry->data_blocks = 1;
    } else {
        // دایرکتوری‌ها فضای داده ندارند
        entry->data_offset = 0;
        entry->data_blocks = 0;
    }
    
    state->superblock->file_count++;
    
    printf("Created new %s: %s\n", (type == 1) ? "directory" : "file", filename);
    return 0;
}

// تغییر سایز فایل
int fs_resize_file(file_entry_t *entry, uint32_t new_size, struct fs_state *state) {
    if (!entry || !state) return -EINVAL;
    
    uint32_t old_blocks = entry->data_blocks;
    uint32_t new_blocks = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    printf("Resizing file from %u to %u bytes (%u to %u blocks)\n", 
           entry->size, new_size, old_blocks, new_blocks);
    
    if (new_blocks == old_blocks) {
        entry->size = new_size;
        entry->mtime = time(NULL);
        return 0;
    }
    
    if (new_blocks > old_blocks) {
        // نیاز به بلوک‌های بیشتر
        uint32_t additional_blocks = new_blocks - old_blocks;
        uint32_t start_block;
        
        // سعی می‌کنیم بلوک‌های مجاور اختصاص دهیم
        if (fs_alloc_blocks(additional_blocks, state, &start_block) < 0) {
            // اگر موفق نشدیم، کل فضای جدید را اختصاص می‌دهیم
            if (fs_alloc_blocks(new_blocks, state, &start_block) < 0) {
                return -ENOSPC;
            }
            
            // بلوک‌های قدیمی را آزاد می‌کنیم
            if (old_blocks > 0) {
                uint32_t old_start_block = entry->data_offset / BLOCK_SIZE;
                fs_free_blocks(old_start_block, old_blocks, state);
            }
            
            entry->data_offset = start_block * BLOCK_SIZE;
        } else {
            // اگر بلوک‌های اختصاص داده شده مجاور نباشند، نیاز به جابجایی داده داریم
            uint32_t new_start_block = start_block;
            if (new_start_block != (entry->data_offset / BLOCK_SIZE) + old_blocks) {
                // نیاز به کپی داده
                char *old_data = (char *)state->data + entry->data_offset;
                char *new_data = (char *)state->data + (new_start_block * BLOCK_SIZE);
                
                // کپی داده قدیمی
                memcpy(new_data, old_data, entry->size);
                
                // آزادسازی بلوک‌های قدیمی
                uint32_t old_start_block = entry->data_offset / BLOCK_SIZE;
                fs_free_blocks(old_start_block, old_blocks, state);
                
                entry->data_offset = new_start_block * BLOCK_SIZE;
            }
        }
    } else {
        // آزادسازی بلوک‌های اضافی
        uint32_t blocks_to_free = old_blocks - new_blocks;
        uint32_t start_block_to_free = (entry->data_offset / BLOCK_SIZE) + new_blocks;
        
        fs_free_blocks(start_block_to_free, blocks_to_free, state);
    }
    
    entry->size = new_size;
    entry->data_blocks = new_blocks;
    entry->mtime = time(NULL);
    
    return 0;
}

// ==================== توابع FUSE ====================

int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    memset(stbuf, 0, sizeof(struct stat));
    
    file_entry_t *entry = fs_find_file(path, state);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    stbuf->st_uid = entry->uid;
    stbuf->st_gid = entry->gid;
    stbuf->st_atime = entry->atime;
    stbuf->st_mtime = entry->mtime;
    stbuf->st_ctime = entry->ctime;
    stbuf->st_mode = entry->permissions;
    stbuf->st_size = entry->size;
    stbuf->st_blocks = entry->data_blocks;
    stbuf->st_blksize = BLOCK_SIZE;
    
    if (entry->type == 1) {
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode |= S_IFREG;
        stbuf->st_nlink = 1;
    }
    
    return 0;
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    if (strcmp(path, "/") == 0) {
        for (uint32_t i = 0; i < state->superblock->file_count; i++) {
            filler(buf, state->file_table[i].name, NULL, 0, 0);
        }
    }
    
    return 0;
}

int fs_open(const char *path, struct fuse_file_info *fi) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *entry = fs_find_file(path, state);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    if (entry->type == 1 && (fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EISDIR;
    }
    
    entry->atime = time(NULL);
    return 0;
}

int fs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    (void) fi;
    
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *entry = fs_find_file(path, state);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    if (offset >= entry->size) {
        return 0;
    }
    
    if (offset + size > entry->size) {
        size = entry->size - offset;
    }
    
    char *data_ptr = (char *)state->data + entry->data_offset + offset;
    memcpy(buf, data_ptr, size);
    
    entry->atime = time(NULL);
    return size;
}

int fs_write(const char *path, const char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
    (void) fi;
    
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *entry = fs_find_file(path, state);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    if (entry->type == 1) {
        return -EISDIR;
    }
    
    size_t new_size = offset + size;
    if (new_size > entry->size) {
        int res = fs_resize_file(entry, new_size, state);
        if (res < 0) {
            return res;
        }
    }
    
    char *data_ptr = (char *)state->data + entry->data_offset + offset;
    memcpy(data_ptr, buf, size);
    
    entry->mtime = time(NULL);
    return size;
}

int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    int res = fs_create_file(path, mode, 0, state);
    if (res < 0) {
        return res;
    }
    
    return fs_open(path, fi);
}

int fs_mkdir(const char *path, mode_t mode) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    return fs_create_file(path, mode | S_IFDIR, 1, state);
}

int fs_unlink(const char *path) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    const char *filename = path + 1;
    file_entry_t *table = state->file_table;
    uint32_t count = state->superblock->file_count;
    
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(table[i].name, filename) == 0) {
            if (table[i].type == 1) {
                return -EISDIR;
            }
            
            // آزادسازی بلوک‌های فایل
            if (table[i].data_blocks > 0) {
                uint32_t start_block = table[i].data_offset / BLOCK_SIZE;
                fs_free_blocks(start_block, table[i].data_blocks, state);
            }
            
            memmove(&table[i], &table[i + 1], (count - i - 1) * sizeof(file_entry_t));
            state->superblock->file_count--;
            
            printf("Deleted file: %s\n", filename);
            return 0;
        }
    }
    
    return -ENOENT;
}

int fs_rmdir(const char *path) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    const char *dirname = path + 1;
    file_entry_t *table = state->file_table;
    uint32_t count = state->superblock->file_count;
    
    // بررسی می‌کنیم که دایرکتوری خالی باشد
    // (در نسخه ساده، همه فایل‌ها در ریشه هستند)
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(table[i].name, dirname) == 0 && table[i].type == 1) {
            memmove(&table[i], &table[i + 1], (count - i - 1) * sizeof(file_entry_t));
            state->superblock->file_count--;
            
            printf("Deleted directory: %s\n", dirname);
            return 0;
        }
    }
    
    return -ENOENT;
}

int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *entry = fs_find_file(path, state);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    if (entry->type == 1) {
        return -EISDIR;
    }
    
    return fs_resize_file(entry, size, state);
}

int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *entry = fs_find_file(path, state);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    entry->atime = tv[0].tv_sec;
    entry->mtime = tv[1].tv_sec;
    
    return 0;
}