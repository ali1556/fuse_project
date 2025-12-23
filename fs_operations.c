#include "general_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// پیدا کردن فایل بر اساس مسیر
file_entry_t *fs_find_file(const char *path, struct fs_state *state) {
    if (strcmp(path, "/") == 0) {
        static file_entry_t root_dir;
        memset(&root_dir, 0, sizeof(root_dir));
        strcpy(root_dir.name, "/");
        root_dir.type = 1;
        root_dir.permissions = 0755;
        root_dir.size = BLOCK_SIZE;
        root_dir.uid = 0;  // root
        root_dir.gid = 0;  // root group
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

// پیدا کردن کاربر بر اساس نام
user_entry_t *fs_find_user(const char *username, struct fs_state *state) {
    if (!state || !username) return NULL;
    
    for (uint32_t i = 0; i < state->superblock->user_count; i++) {
        if (strcmp(state->user_table[i].username, username) == 0) {
            return &state->user_table[i];
        }
    }
    
    return NULL;
}

// پیدا کردن کاربر بر اساس UID
user_entry_t *fs_find_user_by_uid(uint32_t uid, struct fs_state *state) {
    if (!state) return NULL;
    
    for (uint32_t i = 0; i < state->superblock->user_count; i++) {
        if (state->user_table[i].uid == uid) {
            return &state->user_table[i];
        }
    }
    
    return NULL;
}

// پیدا کردن گروه بر اساس نام
group_entry_t *fs_find_group(const char *groupname, struct fs_state *state) {
    if (!state || !groupname) return NULL;
    
    for (uint32_t i = 0; i < state->superblock->group_count; i++) {
        if (strcmp(state->group_table[i].groupname, groupname) == 0) {
            return &state->group_table[i];
        }
    }
    
    return NULL;
}

// پیدا کردن گروه بر اساس GID
group_entry_t *fs_find_group_by_gid(uint32_t gid, struct fs_state *state) {
    if (!state) return NULL;
    
    for (uint32_t i = 0; i < state->superblock->group_count; i++) {
        if (state->group_table[i].gid == gid) {
            return &state->group_table[i];
        }
    }
    
    return NULL;
}

// بررسی دسترسی کاربر به فایل
int fs_check_permission(file_entry_t *file, uint32_t uid, uint32_t gid, uint32_t required_perms) {
    if (!file) return -ENOENT;
    
    // کاربر root به همه چیز دسترسی دارد
    if (uid == 0) {
        return 0;
    }
    
    uint32_t actual_perms = 0;
    
    // بررسی دسترسی مالک
    if (file->uid == uid) {
        actual_perms = (file->permissions >> 6) & 0x7;  // بیت‌های 6-8: مالک
    }
    // بررسی دسترسی گروه
    else if (file->gid == gid) {
        actual_perms = (file->permissions >> 3) & 0x7;  // بیت‌های 3-5: گروه
    }
    // بررسی دسترسی دیگران
    else {
        actual_perms = file->permissions & 0x7;  // بیت‌های 0-2: دیگران
    }
    
    // بررسی اینکه آیا تمام مجوزهای مورد نیاز وجود دارند
    if ((actual_perms & required_perms) == required_perms) {
        return 0;  // دسترسی مجاز
    }
    
    return -EACCES;  // دسترسی ممنوع
}

// بررسی دسترسی قبل از انجام عملیات
int fs_check_access(const char *path, uint32_t required_perms) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *file = fs_find_file(path, state);
    if (!file) {
        return -ENOENT;
    }
    
    uint32_t uid = getuid();
    uint32_t gid = getgid();
    
    return fs_check_permission(file, uid, gid, required_perms);
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
    entry->name[MAX_FILENAME - 1] = '\0';
    entry->type = type;
    entry->permissions = mode & 0777;
    entry->size = 0;
    entry->uid = getuid();  // مالک فعلی
    entry->gid = getgid();  // گروه فعلی
    entry->uid = entry->uid;
    entry->gid = entry->gid;
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
    
    printf("Created new %s: %s (UID: %u, GID: %u, Perm: %o)\n", 
           (type == 1) ? "directory" : "file", 
           filename, entry->uid, entry->gid, entry->permissions);
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
    stbuf->st_nlink = 1;
    
    if (entry->type == 1) {
        stbuf->st_mode |= S_IFDIR;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode |= S_IFREG;
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

    // بررسی دسترسی خواندن دایرکتوری
    if (fs_check_access(path, 4) < 0) {  // 4 = خواندن
        return -EACCES;
    }

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
    
    // بررسی دسترسی بر اساس نوع عملیات
    uint32_t required_perms = 0;
    int accmode = fi->flags & O_ACCMODE;
    
    if (accmode == O_RDONLY) {
        required_perms = 4;  // خواندن
    } else if (accmode == O_WRONLY || accmode == O_RDWR) {
        required_perms = 2;  // نوشتن
    }
    
    if (fs_check_permission(entry, getuid(), getgid(), required_perms) < 0) {
        return -EACCES;
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
    
    // بررسی دسترسی خواندن
    if (fs_check_permission(entry, getuid(), getgid(), 4) < 0) {
        return -EACCES;
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
    
    // بررسی دسترسی نوشتن
    if (fs_check_permission(entry, getuid(), getgid(), 2) < 0) {
        return -EACCES;
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
    
    // بررسی دسترسی ایجاد در دایرکتوری والد
    char parent_path[1024];
    strcpy(parent_path, path);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash && last_slash != parent_path) {
        *last_slash = '\0';
        if (strlen(parent_path) == 0) {
            strcpy(parent_path, "/");
        }
        if (fs_check_access(parent_path, 2) < 0) {  // نوشتن در دایرکتوری
            return -EACCES;
        }
    }
    
    int res = fs_create_file(path, mode, 0, state);
    if (res < 0) {
        return res;
    }
    
    return fs_open(path, fi);
}

int fs_mkdir(const char *path, mode_t mode) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    // بررسی دسترسی ایجاد دایرکتوری در والد
    char parent_path[1024];
    strcpy(parent_path, path);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash && last_slash != parent_path) {
        *last_slash = '\0';
        if (strlen(parent_path) == 0) {
            strcpy(parent_path, "/");
        }
        if (fs_check_access(parent_path, 2) < 0) {  // نوشتن در دایرکتوری
            return -EACCES;
        }
    }
    
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
            
            // بررسی دسترسی حذف
            if (fs_check_permission(&table[i], getuid(), getgid(), 2) < 0) {
                return -EACCES;
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
            
            // بررسی دسترسی حذف
            if (fs_check_permission(&table[i], getuid(), getgid(), 2) < 0) {
                return -EACCES;
            }
            
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
    
    // بررسی دسترسی نوشتن
    if (fs_check_permission(entry, getuid(), getgid(), 2) < 0) {
        return -EACCES;
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
    
    // فقط مالک یا root می‌تواند زمان فایل را تغییر دهد
    uint32_t uid = getuid();
    if (uid != entry->uid && uid != 0) {
        return -EPERM;
    }
    
    entry->atime = tv[0].tv_sec;
    entry->mtime = tv[1].tv_sec;
    
    return 0;
}

int fs_access(const char *path, int mask) {
    struct fs_state *state = get_fs_state();
    if (!state) return -EIO;
    
    file_entry_t *file = fs_find_file(path, state);
    if (!file) {
        return -ENOENT;
    }
    
    uint32_t uid = getuid();
    uint32_t gid = getgid();
    uint32_t required_perms = 0;
    
    // تبدیل mask به مجوزهای ما
    if (mask & R_OK) required_perms |= 4;  // خواندن
    if (mask & W_OK) required_perms |= 2;  // نوشتن
    if (mask & X_OK) required_perms |= 1;  // اجرا
    
    return fs_check_permission(file, uid, gid, required_perms);
}

// توابع مدیریت دسترسی‌ها (برای CLI)

int fs_chmod(const char *path, mode_t mode, struct fs_state *state) {
    if (!state || !path) return -EINVAL;
    
    file_entry_t *file = fs_find_file(path, state);
    if (!file) {
        return -ENOENT;
    }
    
    // فقط مالک فایل یا root می‌تواند مجوزها را تغییر دهد
    uint32_t current_uid = getuid();
    if (current_uid != file->uid && current_uid != 0) {
        return -EPERM;
    }
    
    file->permissions = mode & 0777;  // فقط 9 بیت آخر
    file->mtime = time(NULL);
    
    printf("Permissions changed for %s: %o\n", path, file->permissions);
    return 0;
}

int fs_chown(const char *path, uint32_t uid, uint32_t gid, struct fs_state *state) {
    if (!state || !path) return -EINVAL;
    
    file_entry_t *file = fs_find_file(path, state);
    if (!file) {
        return -ENOENT;
    }
    
    // فقط root می‌تواند مالکیت را تغییر دهد
    if (getuid() != 0) {
        return -EPERM;
    }
    
    // بررسی وجود کاربر
    if (uid != (uint32_t)-1) {
        user_entry_t *user = fs_find_user_by_uid(uid, state);
        if (!user && uid != 0) {  // uid=0 همیشه root است
            return -ENOENT;  // کاربر وجود ندارد
        }
        file->uid = uid;
    }
    
    // بررسی وجود گروه
    if (gid != (uint32_t)-1) {
        group_entry_t *group = fs_find_group_by_gid(gid, state);
        if (!group && gid != 0) {  // gid=0 همیشه root است
            return -ENOENT;  // گروه وجود ندارد
        }
        file->gid = gid;
    }
    
    file->ctime = time(NULL);
    
    printf("Ownership changed for %s: UID=%u, GID=%u\n", path, file->uid, file->gid);
    return 0;
}

int fs_chgrp(const char *path, uint32_t gid, struct fs_state *state) {
    return fs_chown(path, (uint32_t)-1, gid, state);
}

void fs_print_acl(const char *path, struct fs_state *state) {
    if (!state || !path) {
        printf("Invalid parameters\n");
        return;
    }
    
    file_entry_t *file = fs_find_file(path, state);
    if (!file) {
        printf("File not found: %s\n", path);
        return;
    }
    
    printf("=== ACL for %s ===\n", path);
    
    // نمایش مالک
    user_entry_t *owner = fs_find_user_by_uid(file->uid, state);
    printf("Owner: %s (UID: %u)\n", owner ? owner->username : "unknown", file->uid);
    
    // نمایش گروه
    group_entry_t *group = fs_find_group_by_gid(file->gid, state);
    printf("Group: %s (GID: %u)\n", group ? group->groupname : "unknown", file->gid);
    
    // نمایش مجوزها
    printf("Permissions: %o\n", file->permissions);
    printf("  Owner:  %c%c%c\n",
           (file->permissions & 0400) ? 'r' : '-',
           (file->permissions & 0200) ? 'w' : '-',
           (file->permissions & 0100) ? 'x' : '-');
    printf("  Group:  %c%c%c\n",
           (file->permissions & 0040) ? 'r' : '-',
           (file->permissions & 0020) ? 'w' : '-',
           (file->permissions & 0010) ? 'x' : '-');
    printf("  Others: %c%c%c\n",
           (file->permissions & 0004) ? 'r' : '-',
           (file->permissions & 0002) ? 'w' : '-',
           (file->permissions & 0001) ? 'x' : '-');
    
    // نمایش زمان‌ها
    printf("Last access: %s", ctime((time_t *)&file->atime));
    printf("Last modification: %s", ctime((time_t *)&file->mtime));
    printf("Last status change: %s", ctime((time_t *)&file->ctime));
    printf("========================\n");
}