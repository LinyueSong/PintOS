#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include <list.h>
#include "threads/synch.h"

struct cache_entry {
  struct list_elem elem;
  int dirty_bit;
  block_sector_t sector;
  struct lock lck;
  char data[512];
};

void block_read_cached(struct block* b, block_sector_t sec, void* buffer, int offset, int size);
void block_write_cached(struct block* b, block_sector_t sec, void* buffer, int offset,
                        int size); /* Wrapper around block_write function that implements caching*/
void cache_init();
int hit_rate();
void flush_cache();

#endif /* filesys/inode.h */
