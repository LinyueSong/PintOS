







/* Not used anywhere except here
 * Just a helper struct that records the splitted path
 */
struct split_path {
  char *path_to_dir;   
  char *new_dir_name;
};

struct inode_disk {
  off_t length;         /* File size in bytes. */
  int is_dir;
  unsigned magic;       /* Magic number. */
  block_sector_t direct[123];       /* 123 direct pointer */
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
  struct lock dir_lock;   /* Lock on directory */
  int writers;        /* Indicate  */
};

/* An open file. */
struct file {
  struct inode* inode; /* File's inode. */
  off_t pos;           /* Current position. */
  bool deny_write;     /* Has file_deny_write() been called? */
};