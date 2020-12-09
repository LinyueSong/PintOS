#include "filesys/filesys.h"
#include "filesys/cache.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "filesys/utils.h"

/* Partition that contains the file system. */
struct block* fs_device;

static void do_format(void);
bool split_path_to_directory(const char *path, struct split_path *pt);



/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
  cache_init();
  fs_device = block_get_role(BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC("No file system device found, can't initialize file system.");

  inode_init();
  free_map_init();

  if (format)
    do_format();

  free_map_open();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done(void) {
  flush_cache();
  free_map_close(); 
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create(const char* name, off_t initial_size, int is_dir) {

  block_sector_t inode_sector = 0;
  struct dir* dir;
  struct split_path *pt = (struct split_path*) malloc(sizeof(struct split_path));

  /* Split name in path to dir and name of the file */
  if (!split_path_to_directory(name, pt) || strcmp(name, "") == 0)
    return false;

  if (thread_current()->cwd == NULL)
    dir = dir_open_root();
  else if (!pt->path_to_dir) {
    if (thread_current()->cwd->inode->removed)
      return false;
    dir = dir_reopen(thread_current()->cwd);
  } else
    dir= dir_open(walk_path(pt->path_to_dir));

  /* Acquire a lock on the directory */
  lock_acquire(&dir->inode->dir_lock);
  
  bool success = (dir != NULL && free_map_allocate(1, &inode_sector) &&
                  inode_create(inode_sector, initial_size, is_dir) && dir_add(dir, pt->new_dir_name, inode_sector, is_dir));
  if (!success && inode_sector != 0) {
    free_map_release(inode_sector, 1);
    lock_release(&dir->inode->dir_lock);
    goto done;
  }

  /* Release the lock on the directory */
  lock_release(&dir->inode->dir_lock);

  /* If we create a new directory we add "." and ".." to it */
  if (is_dir && success) {
    block_sector_t self = inode_sector;
    block_sector_t parent = dir->inode->sector;

    block_sector_t d_self = 0;
    block_sector_t d_parent = 0;

    struct dir *new_dir = dir_open(walk_path(name));
    //lock_acquire(&new_dir->inode->dir_lock);
    success = (/*free_map_allocate(1, &d_self) && free_map_allocate(1, &d_parent) && inode_create(d_self, 0, 1)
              && inode_create(d_parent, 0, 1) && */ dir_add(new_dir, ".", self, 1) && dir_add(new_dir, "..", parent, 1));
    //lock_release(&new_dir->inode->dir_lock);
    dir_close(new_dir);
  }

  done:
  dir_close(dir);
  free(pt->path_to_dir);
  free(pt->new_dir_name);
  free(pt);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file* filesys_open(const char* name) {
  struct inode* inode = NULL;
  struct dir* dir; //= dir_open_root();
  struct split_path *pt = (struct split_path*) malloc(sizeof(struct split_path));

  /* Split name in path to dir and name of the file */
  if (!split_path_to_directory(name, pt))
    return false;

  if (thread_current()->cwd == NULL)
    dir = dir_open_root();
  else if (!pt->path_to_dir)
    dir = dir_reopen(thread_current()->cwd);
  else
    dir= dir_open(walk_path(pt->path_to_dir));
  

  if (dir != NULL)
    dir_lookup(dir, pt->new_dir_name, &inode);
  if (pt->path_to_dir && !strcmp(pt->new_dir_name, ".") && !strcmp(pt->path_to_dir, "/")) {
    inode = dir->inode;
  }
  dir_close(dir);

  if (inode == NULL) {
    return NULL;
  }

  struct inode_disk *ind_disk = (struct inode_disk*) malloc(sizeof(struct inode_disk));
  block_read_cached(fs_device, inode->sector, ind_disk, 0, sizeof(struct inode_disk));

  if (ind_disk->is_dir){
    free(ind_disk);
    free(pt->path_to_dir);
    free(pt->new_dir_name);
    free(pt);
    return dir_open(inode);
  } else {
    free(ind_disk);
    free(pt->path_to_dir);
    free(pt->new_dir_name);
    free(pt);
    if(inode->sector > 322229) {
    //barrier();
      PANIC("PANIC PANIC PANIC \n\n\n\n\n\n");
    }
    return file_open(inode);
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove(const char* name) {
    struct dir* dir;//= dir_open_root();
  struct split_path *pt = (struct split_path*) malloc(sizeof(struct split_path));

  /* Split name in path to dir and name of the file */
  if (!split_path_to_directory(name, pt))
    return false;

  // char *parent_dir = malloc(sizeof(char) * (strlen(name) + 4));
  // memcpy(parent_dir, name, strlen(name));
  // parent_dir[strlen(name)] = '/';
  // parent_dir[strlen(name)+1] = '.';
  // parent_dir[strlen(name)+2] = '.';
  // parent_dir[strlen(name)+3] = '\0';
  if (thread_current()->cwd == NULL)
    dir = dir_open_root();
  else if (!pt->path_to_dir)
    dir = dir_reopen(thread_current()->cwd);
  else
    dir= dir_open(walk_path(pt->path_to_dir));


  bool success = dir != NULL && dir_remove(dir, pt->new_dir_name);
  dir_close(dir);

  free(pt->path_to_dir);
  free(pt->new_dir_name);
  free(pt);
  return success;
}

/* Formats the file system. */
static void do_format(void) {
  printf("Formatting file system...");
  free_map_create();
  if (!dir_create(ROOT_DIR_SECTOR, 16))
    PANIC("root directory creation failed");
  free_map_close();
  printf("done.\n");
}

/* HELPER FUNCTION 
 * sets the file_des->is_dir based on what the inode is.
 */
void set_is_dir(struct file_descriptor *file_des) {
  void *f_ptr = file_des->f_ptr;
  struct inode_disk *ind_disk = (struct inode_disk*) malloc(sizeof(struct inode_disk));
  struct file *ret = (struct file*) file_des->f_ptr;
  struct inode *ind = (struct inode*)ret->inode;
  block_read_cached(fs_device, ind->sector, ind_disk, 0, sizeof(struct inode_disk));
  file_des->is_dir = ind_disk->is_dir;

  free(ind_disk);
}

/* HELPER FUNCTION 
 * Walk through the path and return the inode corresponding to the NAME
 */
struct inode *walk_path(char *name) {
  struct thread *t = thread_current();
  struct dir *cur_dir;
  struct dir *parent = NULL;
  char next_dir[NAME_MAX + 1];
  int i;
  if (name[0] == '/' || t->cwd == NULL) 
    cur_dir = dir_open_root();
  else 
    cur_dir = dir_reopen(t->cwd);

  struct inode *cur_inode = cur_dir->inode;

  while ((i = get_next_part(next_dir, &name)) != 0) {

    if (i == -1) {
      return NULL;
    }

    if (!dir_lookup(cur_dir, &next_dir, &cur_inode)) {
      dir_close(cur_dir);
      return NULL;
    }
    else {
      //printf("I'm null curinode in walk path: name is: %s    and cur_inode->sector is: %d\n ", name, cur_inode->sector);
      dir_close(cur_dir);
      // if (!parent)
      //   dir_close(parent);
      // parent = cur_dir;
      cur_dir = dir_open(cur_inode);
    }
  }
  // if (!parent)
  //   dir_close(parent);
  //dir_close(cur_dir);
  return cur_inode;
}

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
next call will return the next file name part. Returns 1 if successful, 0 at
end of string, -1 for a too-long file name part. */
int get_next_part (char part[NAME_MAX + 1], const char **srcp) {
const char *src = *srcp;
char *dst = part;
/* Skip leading slashes. If it’s all slashes, we’re done. */
while (*src == '/')
  src++;
  if (*src == '\0')
    return 0;
/* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
while (*src != '/' && *src != '\0') {
  if (dst < part + NAME_MAX)
    *dst++ = *src;
  else
    return -1;
  src++;
}
*dst = '\0';
/* Advance source pointer. */
*srcp = src;
return 1;
}


bool split_path_to_directory(const char *path, struct split_path *pt) {
  bool success = false;

  if (!strchr(path, '/')) {
    pt->path_to_dir = NULL;
    pt->new_dir_name = (char*)malloc(sizeof(char) * (strlen(path) + 1));
    memcpy(pt->new_dir_name, path, strlen(path));
    pt->new_dir_name[strlen(path)] = '\0';
    return true;
  } else {
    size_t size_of_path = 0;
    size_t size_of_dir = 0;
    bool done_with_path = false;
    char *ptr = path;
    char *dir_name;

    /* Find the length of the path and dir name */
    for (; *ptr != 0; ptr++) {
      if (strchr(ptr, '/') == NULL)
        done_with_path = true;
      if (done_with_path)
        size_of_dir++;
      else {
        size_of_path++;
        dir_name = ptr;
      }
    }
    pt->new_dir_name = (char*) malloc(sizeof(char) * (size_of_dir + 1));
    pt->path_to_dir = (char*) malloc(sizeof(char) * (size_of_path + 1));
    memcpy(pt->path_to_dir, path, size_of_path);
    memcpy(pt->new_dir_name, dir_name+1, size_of_dir);
    pt->path_to_dir[size_of_path] = '\0';
    pt->new_dir_name[size_of_dir] = '\0';

    if (!strcmp(pt->new_dir_name, "")) {
      free(pt->new_dir_name);
      pt->new_dir_name = (char*)malloc(sizeof(char) * 2);
      pt->new_dir_name[0] = '.';
      pt->new_dir_name[1] = '\0';
    }

    return true;
  }
  return false;
}


