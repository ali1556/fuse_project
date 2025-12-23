#include "general_fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_cli_help() {
    printf("General FS Management Commands:\n");
    printf("  useradd <username>          - Add new user\n");
    printf("  userdel <username>          - Delete user\n");
    printf("  groupadd <groupname>        - Add new group\n");
    printf("  groupdel <groupname>        - Delete group\n");
    printf("  usermod -aG <group> <user>  - Add user to group\n");
    printf("  chmod <mode> <path>         - Change file permissions\n");
    printf("  chown <user>:<group> <path> - Change file ownership\n");
    printf("  chgrp <group> <path>        - Change file group\n");
    printf("  getfacl <path>              - Show file ACL\n");
    printf("  listusers                   - List all users\n");
    printf("  listgroups                  - List all groups\n");
}

int handle_cli_command(int argc, char *argv[], struct fs_state *state) {
    if (argc < 2) {
        print_cli_help();
        return 0;
    }
    
    char *command = argv[1];
    
    if (strcmp(command, "useradd") == 0) {
        if (argc != 3) {
            printf("Usage: useradd <username>\n");
            return -1;
        }
        // پیدا کردن UID جدید
        uint32_t new_uid = state->superblock->user_count + 1000;
        return fs_add_user(argv[2], new_uid, new_uid, state);
    }
    else if (strcmp(command, "userdel") == 0) {
        if (argc != 3) {
            printf("Usage: userdel <username>\n");
            return -1;
        }
        return fs_delete_user(argv[2], state);
    }
    else if (strcmp(command, "groupadd") == 0) {
        if (argc != 3) {
            printf("Usage: groupadd <groupname>\n");
            return -1;
        }
        // پیدا کردن GID جدید
        uint32_t new_gid = state->superblock->group_count + 1000;
        return fs_add_group(argv[2], new_gid, state);
    }
    else if (strcmp(command, "groupdel") == 0) {
        if (argc != 3) {
            printf("Usage: groupdel <groupname>\n");
            return -1;
        }
        return fs_delete_group(argv[2], state);
    }
    else if (strcmp(command, "usermod") == 0) {
        if (argc != 5 || strcmp(argv[2], "-aG") != 0) {
            printf("Usage: usermod -aG <group> <user>\n");
            return -1;
        }
        return fs_add_user_to_group(argv[4], argv[3], state);
    }
    else if (strcmp(command, "chmod") == 0) {
        if (argc != 4) {
            printf("Usage: chmod <mode> <path>\n");
            return -1;
        }
        mode_t mode = strtol(argv[2], NULL, 8);
        return fs_chmod(argv[3], mode, state);
    }
    else if (strcmp(command, "chown") == 0) {
        if (argc != 4) {
            printf("Usage: chown <user>:<group> <path>\n");
            return -1;
        }
        // تجزیه user:group
        char *colon = strchr(argv[2], ':');
        uint32_t uid = -1, gid = -1;
        
        if (colon) {
            *colon = '\0';
            user_entry_t *user = fs_find_user(argv[2], state);
            if (user) uid = user->uid;
            group_entry_t *group = fs_find_group(colon + 1, state);
            if (group) gid = group->gid;
        } else {
            user_entry_t *user = fs_find_user(argv[2], state);
            if (user) uid = user->uid;
        }
        
        return fs_chown(argv[3], uid, gid, state);
    }
    else if (strcmp(command, "chgrp") == 0) {
        if (argc != 4) {
            printf("Usage: chgrp <group> <path>\n");
            return -1;
        }
        group_entry_t *group = fs_find_group(argv[2], state);
        if (!group) {
            printf("Group not found: %s\n", argv[2]);
            return -1;
        }
        return fs_chgrp(argv[3], group->gid, state);
    }
    else if (strcmp(command, "getfacl") == 0) {
        if (argc != 3) {
            printf("Usage: getfacl <path>\n");
            return -1;
        }
        fs_print_acl(argv[2], state);
        return 0;
    }
    else if (strcmp(command, "listusers") == 0) {
        printf("=== Users ===\n");
        for (uint32_t i = 0; i < state->superblock->user_count; i++) {
            user_entry_t *user = &state->user_table[i];
            printf("%s (UID: %u, GID: %u, Root: %s)\n",
                   user->username, user->uid, user->gid,
                   user->is_root ? "yes" : "no");
        }
        return 0;
    }
    else if (strcmp(command, "listgroups") == 0) {
        printf("=== Groups ===\n");
        for (uint32_t i = 0; i < state->superblock->group_count; i++) {
            group_entry_t *group = &state->group_table[i];
            printf("%s (GID: %u, Members: %u)\n",
                   group->groupname, group->gid, group->member_count);
        }
        return 0;
    }
    else {
        printf("Unknown command: %s\n", command);
        print_cli_help();
        return -1;
    }
}