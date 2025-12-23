#include "general_fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// مقداردهی اولیه کاربران و گروه‌ها
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

// اضافه کردن کاربر جدید
int fs_add_user(const char *username, uint32_t uid, uint32_t gid, struct fs_state *state) {
    if (!state || !username || state->superblock->user_count >= MAX_USERS) {
        return -ENOSPC;
    }
    
    // بررسی وجود کاربر با همین نام
    if (fs_find_user(username, state) != NULL) {
        return -EEXIST;
    }
    
    // بررسی وجود UID تکراری
    if (fs_find_user_by_uid(uid, state) != NULL) {
        return -EEXIST;
    }
    
    user_entry_t *new_user = &state->user_table[state->superblock->user_count];
    strncpy(new_user->username, username, MAX_USERNAME - 1);
    new_user->username[MAX_USERNAME - 1] = '\0';
    new_user->uid = uid;
    new_user->gid = gid;
    new_user->gids[0] = gid;  // گروه اصلی
    new_user->gid_count = 1;
    new_user->is_root = 0;
    
    state->superblock->user_count++;
    printf("User added: %s (UID: %u, GID: %u)\n", username, uid, gid);
    return 0;
}

// حذف کاربر
int fs_delete_user(const char *username, struct fs_state *state) {
    if (!state || !username) return -EINVAL;
    
    user_entry_t *user = fs_find_user(username, state);
    if (!user) {
        return -ENOENT;
    }
    
    // نمی‌توان کاربر root را حذف کرد
    if (user->is_root) {
        return -EPERM;
    }
    
    // حذف کاربر از همه گروه‌ها
    for (uint32_t i = 0; i < state->superblock->group_count; i++) {
        group_entry_t *group = &state->group_table[i];
        for (uint8_t j = 0; j < group->member_count; j++) {
            if (group->members[j] == user->uid) {
                // جابجایی اعضا برای حذف
                for (uint8_t k = j; k < group->member_count - 1; k++) {
                    group->members[k] = group->members[k + 1];
                }
                group->member_count--;
                break;
            }
        }
    }
    
    // حذف کاربر از جدول
    uint32_t index = user - state->user_table;
    for (uint32_t i = index; i < state->superblock->user_count - 1; i++) {
        memcpy(&state->user_table[i], &state->user_table[i + 1], sizeof(user_entry_t));
    }
    state->superblock->user_count--;
    
    printf("User deleted: %s\n", username);
    return 0;
}

// اضافه کردن گروه جدید
int fs_add_group(const char *groupname, uint32_t gid, struct fs_state *state) {
    if (!state || !groupname || state->superblock->group_count >= MAX_GROUPS) {
        return -ENOSPC;
    }
    
    // بررسی وجود گروه با همین نام
    if (fs_find_group(groupname, state) != NULL) {
        return -EEXIST;
    }
    
    // بررسی وجود GID تکراری
    if (fs_find_group_by_gid(gid, state) != NULL) {
        return -EEXIST;
    }
    
    group_entry_t *new_group = &state->group_table[state->superblock->group_count];
    strncpy(new_group->groupname, groupname, MAX_GROUPNAME - 1);
    new_group->groupname[MAX_GROUPNAME - 1] = '\0';
    new_group->gid = gid;
    new_group->member_count = 0;
    
    state->superblock->group_count++;
    printf("Group added: %s (GID: %u)\n", groupname, gid);
    return 0;
}

// حذف گروه
int fs_delete_group(const char *groupname, struct fs_state *state) {
    if (!state || !groupname) return -EINVAL;
    
    group_entry_t *group = fs_find_group(groupname, state);
    if (!group) {
        return -ENOENT;
    }
    
    // نمی‌توان گروه root را حذف کرد
    if (group->gid == 0) {
        return -EPERM;
    }
    
    // حذف گروه از جدول
    uint32_t index = group - state->group_table;
    for (uint32_t i = index; i < state->superblock->group_count - 1; i++) {
        memcpy(&state->group_table[i], &state->group_table[i + 1], sizeof(group_entry_t));
    }
    state->superblock->group_count--;
    
    printf("Group deleted: %s\n", groupname);
    return 0;
}

// اضافه کردن کاربر به گروه
int fs_add_user_to_group(const char *username, const char *groupname, struct fs_state *state) {
    if (!state || !username || !groupname) return -EINVAL;
    
    user_entry_t *user = fs_find_user(username, state);
    group_entry_t *group = fs_find_group(groupname, state);
    
    if (!user) return -ENOENT;
    if (!group) return -ENOENT;
    
    // بررسی آیا کاربر قبلاً در گروه است
    for (uint8_t i = 0; i < group->member_count; i++) {
        if (group->members[i] == user->uid) {
            return -EEXIST;  // کاربر قبلاً در گروه است
        }
    }
    
    // بررسی ظرفیت گروه
    if (group->member_count >= 50) {
        return -ENOSPC;
    }
    
    // اضافه کردن کاربر به گروه
    group->members[group->member_count++] = user->uid;
    
    // اضافه کردن گروه به لیست گروه‌های کاربر
    if (user->gid_count < 10) {
        user->gids[user->gid_count++] = group->gid;
    }
    
    printf("User %s added to group %s\n", username, groupname);
    return 0;
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