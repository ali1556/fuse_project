#include "general_fs.h"

// پیدا کردن فایل بر اساس مسیر
file_entry_t *fs_find_file(const char *path) {
    if (strcmp(path, "/") == 0) {
        // پوشه ریشه
        static file_entry_t root_dir;
        memset(&root_dir, 0, sizeof(root_dir));
        strcpy(root_dir.name, "/");
        root_dir.type = 1; // directory
        root_dir.permissions = 0755;
        root_dir.size = BLOCK_SIZE;
        root_dir.uid = getuid();
        root_dir.gid = getgid();
        root_dir.atime = root_dir.mtime = root_dir.ctime = time(NULL);
        return &root_dir;
    }

    // حذف / از اول مسیر
    const char *filename = path + 1;
    
    for (int i = 0; i < FS_DATA->superblock->file_count; i++) {
        if (strcmp(FS_DATA->file_table[i].name, filename) == 0) {
            return &FS_DATA->file_table[i];
        }
    }
    
    return NULL;
}

// ایجاد فایل جدید
int fs_create_file(const char *path, mode_t mode, uint32_t type) {
    if (FS_DATA->superblock->file_count >= MAX_FILES) {
        return -ENOSPC;
    }

    const char *filename = path + 1; // حذف /
    
    // بررسی وجود فایل
    if (fs_find_file(path) != NULL) {
        return -EEXIST;
    }

    file_entry_t *entry = &FS_DATA->file_table[FS_DATA->superblock->file_count];
    
    // پر کردن اطلاعات فایل
    strncpy(entry->name, filename, MAX_FILENAME - 1);
    entry->type = type;
    entry->permissions = mode;
    entry->size = 0;
    entry->data_offset = FS_DATA->superblock->last_used_byte;
    entry->data_blocks = 0;
    entry->uid = getuid();
    entry->gid = getgid();
    entry->atime = entry->mtime = entry->ctime = time(NULL);
    
    FS_DATA->superblock->file_count++;
    FS_DATA->superblock->last_used_byte += BLOCK_SIZE;
    
    printf("Created new %s: %s\n", (type == 1) ? "directory" : "file", filename);
    return 0;
}

// تغییر سایز فایل
int fs_resize_file(file_entry_t *entry, uint32_t new_size) {
    uint32_t old_blocks = entry->data_blocks;
    uint32_t new_blocks = (new_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    if (new_blocks > old_blocks) {
        // نیاز به فضای بیشتر
        uint32_t required_space = entry->data_offset + (new_blocks * BLOCK_SIZE);
        if (required_space > FS_SIZE) {
            return -ENOSPC;
        }
        
        // آپدیت last_used_byte اگر لازم باشد
        if (required_space > FS_DATA->superblock->last_used_byte) {
            FS_DATA->superblock->last_used_byte = required_space;
        }
    }
    
    entry->size = new_size;
    entry->data_blocks = new_blocks;
    entry->mtime = time(NULL);
    
    return 0;
}

// ==================== توابع FUSE ====================

int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    
    memset(stbuf, 0, sizeof(struct stat));
    
    file_entry_t *entry = fs_find_file(path);
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
    
    if (entry->type == 1) { // directory
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_nlink = 2;
    } else { // regular file
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

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // فقط برای پوشه ریشه
    if (strcmp(path, "/") == 0) {
        for (int i = 0; i < FS_DATA->superblock->file_count; i++) {
            filler(buf, FS_DATA->file_table[i].name, NULL, 0, 0);
        }
    }
    
    return 0;
}

int fs_open(const char *path, struct fuse_file_info *fi) {
    file_entry_t *entry = fs_find_file(path);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    // برای دایرکتوری‌ها، open مجاز نیست
    if (entry->type == 1 && (fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EISDIR;
    }
    
    // آپدیت زمان دسترسی
    entry->atime = time(NULL);
    
    return 0;
}

int fs_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
    (void) fi;
    
    file_entry_t *entry = fs_find_file(path);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    if (offset >= entry->size) {
        return 0;
    }
    
    if (offset + size > entry->size) {
        size = entry->size - offset;
    }
    
    // کپی داده از دیسک
    char *data_ptr = (char *)FS_DATA->data + entry->data_offset + offset;
    memcpy(buf, data_ptr, size);
    
    // آپدیت زمان دسترسی
    entry->atime = time(NULL);
    
    return size;
}

int fs_write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    (void) fi;
    
    file_entry_t *entry = fs_find_file(path);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    // برای دایرکتوری‌ها، write مجاز نیست
    if (entry->type == 1) {
        return -EISDIR;
    }
    
    // محاسبه سایز جدید
    size_t new_size = offset + size;
    if (new_size > entry->size) {
        int res = fs_resize_file(entry, new_size);
        if (res < 0) {
            return res;
        }
    }
    
    // کپی داده به دیسک
    char *data_ptr = (char *)FS_DATA->data + entry->data_offset + offset;
    memcpy(data_ptr, buf, size);
    
    // آپدیت زمان modification
    entry->mtime = time(NULL);
    
    return size;
}

int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int res = fs_create_file(path, mode, 0); // 0 = regular file
    if (res < 0) {
        return res;
    }
    
    return fs_open(path, fi);
}

int fs_mkdir(const char *path, mode_t mode) {
    return fs_create_file(path, mode | S_IFDIR, 1); // 1 = directory
}

int fs_unlink(const char *path) {
    const char *filename = path + 1;
    file_entry_t *table = FS_DATA->file_table;
    uint32_t count = FS_DATA->superblock->file_count;
    
    for (int i = 0; i < count; i++) {
        if (strcmp(table[i].name, filename) == 0) {
            // فقط فایل‌های معمولی قابل حذف هستند
            if (table[i].type == 1) {
                return -EISDIR;
            }
            
            // حذف فایل با جابجایی بقیه عناصر
            memmove(&table[i], &table[i + 1], (count - i - 1) * sizeof(file_entry_t));
            FS_DATA->superblock->file_count--;
            
            printf("Deleted file: %s\n", filename);
            return 0;
        }
    }
    
    return -ENOENT;
}

int fs_rmdir(const char *path) {
    const char *dirname = path + 1;
    file_entry_t *table = FS_DATA->file_table;
    uint32_t count = FS_DATA->superblock->file_count;
    
    for (int i = 0; i < count; i++) {
        if (strcmp(table[i].name, dirname) == 0 && table[i].type == 1) {
            // حذف دایرکتوری
            memmove(&table[i], &table[i + 1], (count - i - 1) * sizeof(file_entry_t));
            FS_DATA->superblock->file_count--;
            
            printf("Deleted directory: %s\n", dirname);
            return 0;
        }
    }
    
    return -ENOENT;
}

int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    
    file_entry_t *entry = fs_find_file(path);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    // برای دایرکتوری‌ها، truncate مجاز نیست
    if (entry->type == 1) {
        return -EISDIR;
    }
    
    return fs_resize_file(entry, size);
}

int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    
    file_entry_t *entry = fs_find_file(path);
    if (entry == NULL) {
        return -ENOENT;
    }
    
    entry->atime = tv[0].tv_sec;
    entry->mtime = tv[1].tv_sec;
    
    return 0;
}