#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

void block_read_cached(struct block* b, block_sector_t sec, void* buffer, int offset, int size);
void block_write_cached(struct block* b, block_sector_t sec, void* buffer, int offset,
                        int size); /* Wrapper around block_write function that implements caching*/
void cache_init();

#endif /* filesys/inode.h */
