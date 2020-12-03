#include "filesys/cache.h"
#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#define MAXSIZE 64
struct cache_entry* get_cache_entry(
    struct block* b,
    block_sector_t
        sec); /* Returns the cache entry given a sector, if it is not in the cache we bring it in and evict another entry if necessary. */
void LRU_evict(struct block*);

struct cache_entry {
  struct list_elem elem;
  int dirty_bit;
  block_sector_t sector;
  struct lock lck;
  char data[512];
};

struct lock cache_lookup_lock;
struct list cache;

/* Initializes the inode module. */
void cache_init(void) {
  list_init(&cache);
  lock_init(&cache_lookup_lock);
}

void block_read_cached(struct block* b, block_sector_t sec, void* buffer, int offset, int size) {
  struct cache_entry* cache = get_cache_entry(b, sec);
  memcpy(buffer, cache->data + offset, size);
  lock_release(&cache->lck);
}

void block_write_cached(struct block* b, block_sector_t sec, void* buffer, int offset, int size) {
  struct cache_entry* cache = get_cache_entry(b, sec);
  memcpy(cache->data + offset, buffer, size);
  cache->dirty_bit = 1;
  lock_release(&cache->lck);
}

struct cache_entry* get_cache_entry(struct block* b, block_sector_t sec) {
  lock_acquire(&cache_lookup_lock);
  struct list_elem* e;
  struct cache_entry* entry;
  for (e = list_begin(&cache); e != list_end(&cache); e = list_next(e)) {
    entry = list_entry(e, struct cache_entry, elem);
    if (entry->sector == sec) {
      list_remove(e);
      list_push_front(&cache, e);
      lock_acquire(&entry->lck);
      lock_release(&cache_lookup_lock);
      return entry;
    }
  }
  LRU_evict(b);
  entry = (struct cache_entry*)malloc(sizeof(struct cache_entry));
  entry->dirty_bit = 0;
  entry->sector = sec;
  lock_init(&entry->lck);
  lock_acquire(&entry->lck);
  block_read(b, sec, entry->data);
  list_push_front(&cache, &entry->elem);
  lock_release(&cache_lookup_lock);
  return entry;
}

void LRU_evict(struct block* block) {
  if (list_size(&cache) == MAXSIZE) {
    /* ensure nobody reads/write on last entry */
    while (!lock_try_acquire(&list_entry(list_rbegin(&cache), struct cache_entry, elem)->lck))
      ;
    struct list_elem* e = list_pop_back(&cache);
    struct cache_entry* entry = list_entry(e, struct cache_entry, elem);
    if (entry->dirty_bit == 1)
      block_write(block, entry->sector, entry->data);
    free(entry);
  }
}