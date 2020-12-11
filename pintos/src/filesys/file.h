#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"
#include <stdbool.h>

struct inode;


/* An open file. */
struct file {
  struct inode* inode; /* File's inode. */
  off_t pos;           /* Current position. */
  bool deny_write;     /* Has file_deny_write() been called? */
};


/* Opening and closing files. */
struct file* file_open(struct inode* inode);
struct file* file_reopen(struct file* file);
void file_close(struct file* file);
struct inode* file_get_inode(struct file* file);

/* Reading and writing. */
off_t file_read(struct file* file, void* buffer, off_t size);
off_t file_read_at(struct file* file, void* buffer, off_t size, off_t file_ofs);
off_t file_write(struct file* file, const void* buffer, off_t size);
off_t file_write_at(struct file* file, const void* buffer, off_t size, off_t file_ofs);

/* Preventing writes. */
void file_deny_write(struct file* file);
void file_allow_write(struct file* file);

/* File position. */
void file_seek(struct file* file, off_t new_pos);
off_t file_tell(struct file* file);
off_t file_length(struct file* file);

#endif /* filesys/file.h */
