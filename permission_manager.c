#include "general_fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// تغییر مجوزهای فایل
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

// تغییر مالک فایل
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

// تغییر گروه فایل
int fs_chgrp(const char *path, uint32_t gid, struct fs_state *state) {
    return fs_chown(path, (uint32_t)-1, gid, state);
}

// نمایش ACL فایل
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

// تابع access برای بررسی دسترسی
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