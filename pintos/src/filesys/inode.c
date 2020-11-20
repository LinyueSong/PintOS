#include "filesys/inode.h"
#include "filesys/cache.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_MAX 124 * 512
#define INDIRECT_MAX 124 * 512 + 128 * 512
#define DOUBLE_MAX 124 * 512 + 128 * 512 + 128 * 128 * 512

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
  off_t length;         /* File size in bytes. */
  unsigned magic;       /* Magic number. */
  block_sector_t direct[124];       /* 124 direct pointer */
  block_sector_t indirect;          /* indirect_pointer */
  block_sector_t double_indirect;   /* double_indirect_pointer */
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t bytes_to_sectors(off_t size) { return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE); }

/* In-memory inode. */
struct inode {
  struct list_elem elem;  /* Element in inode list. */
  block_sector_t sector;  /* Sector number of disk location. */
  int open_cnt;           /* Number of openers. */
  bool removed;           /* True if deleted, false otherwise. */
  int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
  struct lock resize_lock; /* lock for concurrent resizes */
  struct lock meta_lock;   /* lock for inode metadata */
};

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns 0 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t byte_to_sector(const struct inode* inode, off_t pos) {
  ASSERT(inode != NULL);
  struct inode_disk* di = malloc(BLOCK_SECTOR_SIZE);
  block_read_cached(fs_device, inode->sector, di, 0, BLOCK_SECTOR_SIZE);
  block_sector_t* buffer = malloc(BLOCK_SECTOR_SIZE);
  block_sector_t result = 0;
  /* Traverse pointers to find the corresponding sector based on the position */
  if (pos < DIRECT_MAX) {
    return di->direct[pos / BLOCK_SECTOR_SIZE];
  } else if (pos < INDIRECT_MAX) {
    if (di->indirect != 0) {
      block_read_cached(fs_device, di->indirect, buffer, 0, BLOCK_SECTOR_SIZE);
      result = buffer[(pos - DIRECT_MAX) / BLOCK_SECTOR_SIZE];
    }
  } else if (pos < DOUBLE_MAX) {
    if (di->double_indirect != 0) {
     block_read_cached(fs_device, di->double_indirect, buffer, 0, BLOCK_SECTOR_SIZE);
     block_sector_t sector = buffer[(pos - INDIRECT_MAX) / BLOCK_SECTOR_SIZE/(BLOCK_SECTOR_SIZE/sizeof(block_sector_t))]; 
      if (sector != 0) {
        block_read_cached(fs_device, sector, buffer, 0, BLOCK_SECTOR_SIZE); 
        result =  buffer[((pos - INDIRECT_MAX) / BLOCK_SECTOR_SIZE) % (BLOCK_SECTOR_SIZE/sizeof(block_sector_t))];
      }
    }
  } 
  free(di);
  free(buffer);
  return result;
}

/* Function to resize the inode_disk. May expand or shrink. */
bool inode_resize(block_sector_t id_sector, off_t size) {
  static int zeros[BLOCK_SECTOR_SIZE];
  struct inode_disk * id = malloc(BLOCK_SECTOR_SIZE);
  block_read_cached(fs_device, id_sector, id, 0, BLOCK_SECTOR_SIZE);
  block_sector_t sector;
  /* Direct pointers */
  for (int i = 0; i < 124; i++) {
    /* Shrink */
    if (size <= 512 * i && id->direct[i] != 0) {
      free_map_release(id->direct[i], 1);
      id->direct[i] = 0;
    }
    /* Expand */
    if (size > 512 * i && id->direct[i] == 0) {
      if (!free_map_allocate(1, &sector)) {
      inode_resize(id, id->length);
      free(id);
      return false;
      }
      block_write_cached(fs_device, sector, zeros, 0, BLOCK_SECTOR_SIZE);
      id->direct[i] = sector;
    }
  }
  /* If the direct pointers are sufficient, return */
  if (id->indirect == 0 && size <= 124 * 512) {
    id->length = size;
    block_write_cached(fs_device, id_sector, id, 0, BLOCK_SECTOR_SIZE); 
    free(id);
    return true;
  }
  block_sector_t* buffer = malloc(BLOCK_SECTOR_SIZE);
  /* Allocate a new intermediate sector if not yet */
  if (id->indirect == 0) {
    memset(buffer, 0, 512);
    /* Roll back */
    if (!free_map_allocate(1, &sector)) {
      inode_resize(id, id->length);
      free(buffer);
      free(id);
      return false;
    }
    id->indirect = sector;
  } else {
    /* Read the intermediate sector */
    block_read_cached(fs_device, id->indirect, buffer, 0, BLOCK_SECTOR_SIZE);
  }
  /* Allocate sectors for each pointer on intermediate sector if neceessary*/
  for (int i = 0; i < 128; i++) {
    /* Shrink */
    if (size <= (124 + i) * 512 && buffer[i] != 0) {
      free_map_release(buffer[i], 1);
      buffer[i] = 0;
    }
    /* Expand */
    if (size > (124 + i) * 512 && buffer[i] == 0) {
      if (!free_map_allocate(1, &sector)) { // Handle failure
        inode_resize(id, id->length);
        free(buffer);
        free(id);
        return false;
      }
      block_write_cached(fs_device, sector, zeros, 0, BLOCK_SECTOR_SIZE);
      buffer[i] = sector;
    }
  }
  /* If direct & indirect pointer are sufficient, return */
  if (id->double_indirect == 0 && size <= INDIRECT_MAX) {
    id->length = size;
    block_write_cached(fs_device, id_sector, id, 0, BLOCK_SECTOR_SIZE);
    block_write_cached(fs_device, id->indirect, buffer, 0, BLOCK_SECTOR_SIZE);
    free(buffer);
    free(id);
    return true;
  }
  /* Allocate a new layer 1 intermediate sector if now yet */
  if (id->double_indirect == 0) {
    memset(buffer, 0, 512);
    if (!free_map_allocate(1, &sector)) {
      inode_resize(id, id->length);
      free(buffer);
      free(id);
      return false;
    }
    id->double_indirect = sector;
  } else {
    /* Read the layer 1 intermediate sector if it has already been allocated */
    block_read_cached(fs_device, id->double_indirect, buffer, 0, BLOCK_SECTOR_SIZE);
  }

  /* Allocate layer2 intermediate sector if not yet */
  block_sector_t* buffer2 = malloc(BLOCK_SECTOR_SIZE);
  /* Iterate through the layer 1 intermediate sector */
  for (int i = 0; i < 128; i++) {
    /* Return if all required space has been satisfied. */
    if (buffer[i] == 0 && size <= INDIRECT_MAX + i * 128 * 512) {
      block_write_cached(fs_device, id_sector, id, 0, BLOCK_SECTOR_SIZE);
      block_write_cached(fs_device, id->indirect, buffer, 0, BLOCK_SECTOR_SIZE);
      id->length = size;
      free(buffer);
      free(id);
      free(buffer2);
      return true;
    }
    if (buffer[i] == 0) {
      memset(buffer2, 0, 512);
      if (!free_map_allocate(1, &sector)) {
        inode_resize(id, id->length);
        free(buffer);
        free(buffer2);
        free(id);
        return false;
      }
      buffer[i] = sector;
    } else {
      /* Read layer2 intermediate sector if it has already been allocated */
      block_read_cached(fs_device, buffer[i], buffer2, 0, BLOCK_SECTOR_SIZE);
    }
    /* Iterate through layer2 intermediate sector */
    for (int j = 0; j < 128; j++) {
      /* Shrink */
      if (size <= 124 * 512 + 128 * 512 + i * 128 * 512 + j * 512 && buffer2[j] != 0) {
        free_map_release(buffer2[j], 1);
        buffer2[j] = 0;
      }
      /* Expand */
      if (size > 124 * 512 + 128 * 512 + i * 128 * 512 + j * 512 && buffer2[j] == 0) {
        if (!free_map_allocate(1, &sector)) { // Handle failure
          inode_resize(id, id->length);
          free(buffer);
          free(buffer2);
          free(id);
          return false;
        }
        block_write_cached(fs_device, sector, zeros, 0, BLOCK_SECTOR_SIZE);
        buffer2[j] = sector;
      }
    }
    block_write_cached(fs_device, buffer[i], buffer2, 0, BLOCK_SECTOR_SIZE);
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void) { list_init(&open_inodes); }

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length) {
  struct inode_disk* disk_inode = NULL;
  bool success = false;

  ASSERT(length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    disk_inode->length = 0;
    disk_inode->magic = INODE_MAGIC;
    block_write_cached(fs_device, sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
    success = inode_resize(sector, length);
    free(disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode* inode_open(block_sector_t sector) {
  struct list_elem* e;
  struct inode* inode;

  /* Check whether this inode is already open. */
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes); e = list_next(e)) {
    inode = list_entry(e, struct inode, elem);
    if (inode->sector == sector) {
      inode_reopen(inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front(&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode* inode_reopen(struct inode* inode) {
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode* inode) { return inode->sector; }

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode* inode) {
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0) {
    /* Remove from inode list and release lock. */
    list_remove(&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed) {
      inode_resize(inode->sector, 0);
      free_map_release(inode->sector, 1);
    }
    free(inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove(struct inode* inode) {
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
  uint8_t* buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    block_read_cached(fs_device, sector_idx, buffer + bytes_read, sector_ofs, chunk_size);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode* inode, const void* buffer_, off_t size, off_t offset) {
  const uint8_t* buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  if (inode_length(inode) <= offset + size) {
    inode_resize(inode->sector, size + offset);
  }
  while (size > 0) {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    block_write_cached(fs_device, sector_idx, buffer + bytes_written, sector_ofs, chunk_size);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode* inode) {
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode* inode) {
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode* inode) {
  struct inode_disk* id = malloc(BLOCK_SECTOR_SIZE);
  block_read_cached(fs_device, inode->sector, id, 0, BLOCK_SECTOR_SIZE); 
  off_t result = id->length; 
  free(id);
  return result;
}
