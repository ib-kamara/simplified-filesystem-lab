#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NAME 256
#define BUFFER_SIZE 4096

struct simplefs
{
    char blocks[1024][4096];
    int indirect_blocks[128][12];

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
        long timestamp;
    } inodes[128];

    struct dir_entry
    {
        char name[256];
        int inode_id;
        int is_soft_link;
        char link_path[256];
    } directory[128];

    struct log_entry
    {
        char operation[256];
        long timestamp;
        unsigned int hash;
    } logs[100];

    int block_count;
    int inode_count;
    int dir_count;
    int log_count;
};

extern void init_fs(struct simplefs *fs);
extern int create_file(struct simplefs *fs, const char *name, const char *permissions, int uid, int gid, const char *data);
extern int read_file(struct simplefs *fs, const char *name, int uid, int gid, char *buffer, int max_len);
extern int create_hard_link(struct simplefs *fs, const char *existing_name, const char *new_name, int uid);
extern int create_soft_link(struct simplefs *fs, const char *existing_name, const char *new_name, int uid);
extern void print_logs(struct simplefs *fs);

int main()
{
    struct simplefs fs;
    char buffer[BUFFER_SIZE];

    init_fs(&fs);

    printf("Simplified Filesystem Lab Test\n");
    printf("--------------------------------\n");

    create_file(&fs, "file1.txt", "rw-------", 1001, 2001, "Hello, Filesystem!");

    if (read_file(&fs, "file1.txt", 1001, 2001, buffer, BUFFER_SIZE) > 0)
    {
        printf("Read: %s\n", buffer);
    }
    else
    {
        printf("Read failed\n");
    }

    if (read_file(&fs, "file1.txt", 1002, 3000, buffer, BUFFER_SIZE) < 0)
    {
        printf("Permission denied for UID 1002\n");
    }

    create_hard_link(&fs, "file1.txt", "file1_link.txt", 1001);

    if (read_file(&fs, "file1_link.txt", 1001, 2001, buffer, BUFFER_SIZE) > 0)
    {
        printf("Hard link read: %s\n", buffer);
    }

    create_soft_link(&fs, "file1.txt", "file1_soft.txt", 1001);

    if (read_file(&fs, "file1_soft.txt", 1001, 2001, buffer, BUFFER_SIZE) > 0)
    {
        printf("Soft link read: %s\n", buffer);
    }

    create_file(&fs, "group_file.txt", "rw-r-----", 3001, 5000, "This file can be read by the owner and group.");

    if (read_file(&fs, "group_file.txt", 4000, 5000, buffer, BUFFER_SIZE) > 0)
    {
        printf("Group permission read: %s\n", buffer);
    }
    else
    {
        printf("Group read failed\n");
    }

    char big_data[60000];
    int i;

    for (i = 0; i < 59999; i++)
    {
        big_data[i] = 'A';
    }

    big_data[59999] = '\0';

    create_file(&fs, "big_file.txt", "rw-r--r--", 1001, 2001, big_data);

    if (read_file(&fs, "big_file.txt", 1001, 2001, buffer, BUFFER_SIZE) > 0)
    {
        printf("Big file test passed using indirect blocks\n");
    }

    fs.logs[0].hash = 12345;

    print_logs(&fs);

    return 0;
}