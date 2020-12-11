#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"


struct bitmap;

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_MAX 123 * 512
#define INDIRECT_MAX 123 * 512 + 128 * 512
#define DOUBLE_MAX 123 * 512 + 128 * 512 + 128 * 128 * 512

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
  off_t length;         /* File size in bytes. */
  int is_dir;
  unsigned magic;       /* Magic number. */
  block_sector_t direct[123];       /* 124 direct pointer */
  block_sector_t indirect;          /* indirect_pointer */
  block_sector_t double_indirect;   /* double_indirect_pointer */
};


/* In-memory inode. */
struct inode {
  struct list_elem elem;  /* Element in inode list. */
  block_sector_t sector;  /* Sector number of disk location. */
  int open_cnt;           /* Number of openers. */
  bool removed;           /* True if deleted, false otherwise. */
  int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
  struct lock lookup_lock;/* Lock for lookup the inode disk */
  struct lock meta_lock;  /* Lock for inode metadata */
  struct lock dny_w_lock; /* Lock for deny write cnt */
  struct condition dny_w_cond; /* Condition variable for deny write cnt */
  int writers;            /* Indicate  */
  struct lock dir_lock ;   /* Lock on directory */
};

static inline size_t bytes_to_sectors(off_t size);
static block_sector_t byte_to_sector(const struct inode* inode, off_t pos);
bool inode_resize_unsafe(block_sector_t id_sector, off_t size);
bool inode_resize(struct inode* inode, off_t size);
void inode_init(void);
bool inode_create(block_sector_t sector, off_t length, int is_dir);
struct inode* inode_open(block_sector_t sector);
struct inode* inode_reopen(struct inode* inode);
block_sector_t inode_get_inumber(const struct inode* inode);
void inode_close(struct inode* inode);
void inode_remove(struct inode* inode);
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset);
off_t inode_write_at(struct inode* inode, const void* buffer_, off_t size, off_t offset);
void inode_deny_write(struct inode* inode);
void inode_allow_write(struct inode* inode);
off_t inode_length(const struct inode* inode);

#endif /* filesys/inode.h */
