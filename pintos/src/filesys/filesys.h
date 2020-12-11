#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/directory.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0 /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1 /* Root directory file inode sector. */

/* Block device that contains the file system. */
struct block* fs_device;

/* Not used anywhere except here
 * Just a helper struct that records the splitted path
 */
struct split_path {
  char *path_to_dir;   
  char *new_dir_name;
};

static void do_format(void);
bool split_path_to_directory(const char *path, struct split_path *pt);
void filesys_init(bool format);
void filesys_done(void);
struct dir* get_path_to_dir(struct split_path *pt);
bool filesys_create(const char* name, off_t initial_size, int is_dir);
struct file* filesys_open(const char* name);
bool filesys_remove(const char* name);
static void do_format(void);
struct inode *walk_path(char *name);
int get_next_part (char part[NAME_MAX + 1], const char **srcp);
bool split_path_to_directory(const char *path, struct split_path *pt);

#endif /* filesys/filesys.h */
