#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK_SIZE 4096
#define MAX_BLOCKS 1024
#define MAX_INODES 128
#define MAX_NAME 256
#define MAX_LOGS 100
#define MAX_GROUPS 20

struct inode
{
    int id;
    int size;
    char permissions[10];
    int ref_count;
    int blocks[12];
    int indirect_block;
    int owner_uid;
    int group_id;
    time_t timestamp;
};

struct dir_entry
{
    char name[MAX_NAME];
    int inode_id;
    int is_soft_link;
    char link_path[MAX_NAME];
};

struct log_entry
{
    char operation[MAX_NAME];
    time_t timestamp;
    unsigned int hash;
};

struct simplefs
{
    char blocks[MAX_BLOCKS][BLOCK_SIZE];
    int indirect_blocks[MAX_INODES][12];
    struct inode inodes[MAX_INODES];
    struct dir_entry directory[MAX_INODES];
    struct log_entry logs[MAX_LOGS];
    int block_count;
    int inode_count;
    int dir_count;
    int log_count;
};

unsigned int make_hash(char *text, time_t timestamp)
{
    unsigned int hash = 0;
    int i = 0;

    while (text[i] != '\0')
    {
        hash = hash + text[i];
        i++;
    }

    hash = hash + (unsigned int)timestamp;

    return hash;
}

void add_log(struct simplefs *fs, char *operation)
{
    struct log_entry *log = &fs->logs[fs->log_count % MAX_LOGS];

    strncpy(log->operation, operation, MAX_NAME);
    log->timestamp = time(NULL);
    log->hash = make_hash(log->operation, log->timestamp);

    fs->log_count++;
}

void init_fs(struct simplefs *fs)
{
    memset(fs, 0, sizeof(struct simplefs));
    fs->block_count = 0;
    fs->inode_count = 0;
    fs->dir_count = 0;
    fs->log_count = 0;
}

int create_file(struct simplefs *fs, const char *name, const char *permissions, int uid, int gid, const char *data)
{
    if (fs->inode_count >= MAX_INODES)
    {
        return -1;
    }

    int data_size = strlen(data);
    int needed_blocks = (data_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (needed_blocks > 24)
    {
        return -1;
    }

    if (fs->block_count + needed_blocks >= MAX_BLOCKS)
    {
        return -1;
    }

    struct inode *inode = &fs->inodes[fs->inode_count];

    inode->id = fs->inode_count;
    inode->size = data_size;
    strncpy(inode->permissions, permissions, 10);
    inode->ref_count = 1;
    inode->owner_uid = uid;
    inode->group_id = gid;
    inode->timestamp = time(NULL);
    inode->indirect_block = -1;

    int data_index = 0;
    int i;

    for (i = 0; i < needed_blocks; i++)
    {
        int block_id = fs->block_count++;

        if (i < 12)
        {
            inode->blocks[i] = block_id;
        }
        else
        {
            inode->indirect_block = inode->id;
            fs->indirect_blocks[inode->id][i - 12] = block_id;
        }

        int j;
        for (j = 0; j < BLOCK_SIZE && data_index < data_size; j++)
        {
            fs->blocks[block_id][j] = data[data_index] ^ 0x55;
            data_index++;
        }
    }

    struct dir_entry *entry = &fs->directory[fs->dir_count++];
    strncpy(entry->name, name, MAX_NAME);
    entry->inode_id = inode->id;
    entry->is_soft_link = 0;
    strcpy(entry->link_path, "");

    char message[MAX_NAME];
    snprintf(message, MAX_NAME, "Created file %s by UID %d", name, uid);
    add_log(fs, message);

    fs->inode_count++;

    return inode->id;
}

int has_read_permission(struct inode *inode, int uid, int gid)
{
    if (uid == inode->owner_uid && inode->permissions[0] == 'r')
    {
        return 1;
    }

    if (gid == inode->group_id && inode->permissions[3] == 'r')
    {
        return 1;
    }

    if (inode->permissions[6] == 'r')
    {
        return 1;
    }

    return 0;
}

int read_file(struct simplefs *fs, const char *name, int uid, int gid, char *buffer, int max_len)
{
    int i;

    for (i = 0; i < fs->dir_count; i++)
    {
        if (strcmp(fs->directory[i].name, name) == 0)
        {
            if (fs->directory[i].is_soft_link == 1)
            {
                return read_file(fs, fs->directory[i].link_path, uid, gid, buffer, max_len);
            }

            struct inode *inode = &fs->inodes[fs->directory[i].inode_id];

            if (has_read_permission(inode, uid, gid) == 0)
            {
                return -1;
            }

            int copied = 0;
            int block_number;

            for (block_number = 0; block_number < 24 && copied < inode->size && copied < max_len - 1; block_number++)
            {
                int block_id;

                if (block_number < 12)
                {
                    block_id = inode->blocks[block_number];
                }
                else
                {
                    block_id = fs->indirect_blocks[inode->id][block_number - 12];
                }

                int j;
                for (j = 0; j < BLOCK_SIZE && copied < inode->size && copied < max_len - 1; j++)
                {
                    buffer[copied] = fs->blocks[block_id][j] ^ 0x55;
                    copied++;
                }
            }

            buffer[copied] = '\0';

            char message[MAX_NAME];
            snprintf(message, MAX_NAME, "Read file %s by UID %d", name, uid);
            add_log(fs, message);

            return copied;
        }
    }

    return -1;
}

int create_hard_link(struct simplefs *fs, const char *existing_name, const char *new_name, int uid)
{
    int i;

    for (i = 0; i < fs->dir_count; i++)
    {
        if (strcmp(fs->directory[i].name, existing_name) == 0)
        {
            struct inode *inode = &fs->inodes[fs->directory[i].inode_id];

            if (inode->owner_uid != uid && inode->permissions[7] != 'w')
            {
                return -1;
            }

            inode->ref_count++;

            struct dir_entry *entry = &fs->directory[fs->dir_count++];
            strncpy(entry->name, new_name, MAX_NAME);
            entry->inode_id = inode->id;
            entry->is_soft_link = 0;
            strcpy(entry->link_path, "");

            char message[MAX_NAME];
            snprintf(message, MAX_NAME, "Created hard link %s to %s by UID %d", new_name, existing_name, uid);
            add_log(fs, message);

            return 0;
        }
    }

    return -1;
}

int create_soft_link(struct simplefs *fs, const char *existing_name, const char *new_name, int uid)
{
    struct dir_entry *entry = &fs->directory[fs->dir_count++];

    strncpy(entry->name, new_name, MAX_NAME);
    entry->inode_id = -1;
    entry->is_soft_link = 1;
    strncpy(entry->link_path, existing_name, MAX_NAME);

    char message[MAX_NAME];
    snprintf(message, MAX_NAME, "Created soft link %s to %s by UID %d", new_name, existing_name, uid);
    add_log(fs, message);

    return 0;
}

void print_logs(struct simplefs *fs)
{
    printf("\nFilesystem Logs:\n");

    int i;

    for (i = 0; i < fs->log_count && i < MAX_LOGS; i++)
    {
        unsigned int correct_hash = make_hash(fs->logs[i].operation, fs->logs[i].timestamp);

        printf("[%ld] %s (Hash: %u)", fs->logs[i].timestamp, fs->logs[i].operation, fs->logs[i].hash);

        if (correct_hash != fs->logs[i].hash)
        {
            printf(" TAMPERED");
        }

        printf("\n");
    }
}