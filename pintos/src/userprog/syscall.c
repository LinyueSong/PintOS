#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"
#include "pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "filesys/inode.h"



static void syscall_handler(struct intr_frame*);
void syscall_create(const char* file, unsigned initial_size, struct intr_frame* f);
void syscall_remove(const char* file, struct intr_frame* f);
void syscall_open(const char* file, struct intr_frame* f);
void syscall_filesize(int fd, struct intr_frame* f);
void syscall_read(int fd, void* buffer, unsigned size, struct intr_frame* f);
void syscall_write(int fd, const void* buffer, unsigned size, struct intr_frame* f);
void syscall_seek(int fd, unsigned position, struct intr_frame* f);
void syscall_tell(int fd, struct intr_frame* f);
void syscall_close(int fd, struct intr_frame* f);
void syscall_exit(int status, struct intr_frame* f);
void syscall_chdir(char* name, struct intr_frame *f);
void syscall_readdir(int fd, char *name[NAME_MAX + 1], struct intr_frame *f);
void syscall_inumber(int fd, struct intr_frame *f);
void syscall_isdir(int fd, struct intr_frame *f);
bool valid_fd(int fd_user);
struct file* get_f_ptr(int fd);
struct file_descriptor* get_fd_struct(int fd);
bool check_addr(const void* ptr, int size);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  /* Before calling a syscall helper we must make sure that the arguments passed
   * to us are valid.
   */
  if (!check_addr(args, 4)) {
    syscall_exit(-1, f);
  }
  switch (args[0]) {
    case SYS_CREATE:
      if (!check_addr(args + 4, 8)) {
        syscall_exit(-1, f);
      }
      syscall_create((const char*)args[1], (unsigned)args[2], f);
      break;
    case SYS_REMOVE:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_remove((const char*)args[1], f);
      break;
    case SYS_OPEN:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_open(args[1], f);
      break;
    case SYS_FILESIZE:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_filesize((int)args[1], f);
      break;
    case SYS_READ:
      if (!check_addr(args + 4, 12)) {
        syscall_exit(-1, f);
      }
      syscall_read((int)args[1], (void*)args[2], (unsigned)args[3], f);
      break;
    case SYS_WRITE:
      if (!check_addr(args + 4, 12)) {
        syscall_exit(-1, f);
      }
      syscall_write((int)args[1], (const void*)args[2], (unsigned)args[3], f);
      break;
    case SYS_SEEK:
      if (!check_addr(args + 4, 8)) {
        syscall_exit(-1, f);
      }
      syscall_seek((int)args[1], (unsigned)args[2], f);
      break;
    case SYS_TELL:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_tell((int)args[1], f);
      break;
    case SYS_CLOSE:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_close((int)args[1], f);
      break;
    case SYS_EXIT:
      if (!check_addr(args + 4, 4)) {
        f->eax = -1;
        thread_current()->self->status = -1;
        thread_exit();
      }
      syscall_exit((int)args[1], f);
      break;
    case SYS_EXEC:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_exec(((const char*)args[1]), f);
      break;
    case SYS_WAIT:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_wait((tid_t)args[1], f);
      break;
    case SYS_PRACTICE:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      f->eax = args[1] + 1;
      break;
    case SYS_HALT:
      shutdown_power_off();
      break;
    case SYS_MKDIR:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_mkdir((char*)args[1], f);
      break;
    case SYS_CHDIR:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_chdir((char*) args[1],f);
      break;
    case SYS_READDIR:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_readdir((int) args[1], args[2], f);
      break;
    case SYS_ISDIR:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_isdir((int)args[1], f);
      break;
    case SYS_INUMBER:
      if (!check_addr(args + 4, 4)) {
        syscall_exit(-1, f);
      }
      syscall_inumber((int)args[1], f);
      break;

    default:
      /* PANIC? */
      syscall_exit(-1, f);
  }
}

/* HELPER FUNCTION 
 * that handles the create routine. Creates a file of some size.
 * @file, name of the file
 * @size, size of the file
 * @f, interrupt frame 
 */
void syscall_create(const char* file, unsigned initial_size, struct intr_frame* f) {
  if (!check_addr(file, 0))
    syscall_exit(-1, f);
  f->eax = filesys_create(file, initial_size, 0);
}

/* HELPER FUNCTION 
 * that handles the remove routine. Removes a file.
 * @file, name of the file
 * @f, interrupt frame 
 */
void syscall_remove(const char* file, struct intr_frame* f) {
  if (!check_addr(file, 0))
    syscall_exit(-1, f);
  f->eax = filesys_remove(file);
}

/* HELPER FUNCTION 
 * that handles the open routine. Opens a file.
 * @file, name of the file
 * @f, interrupt frame 
 */
void syscall_open(const char* file, struct intr_frame* f) {
  if (!check_addr(file, -1)) {
    syscall_exit(-1, f);
  }
  if (strcmp(file, "") == 0) {
    f->eax = -1;
    return;
  }
  struct file_descriptor* file_des = malloc(sizeof(struct file_descriptor));
  if (file_des == NULL) {
    f->eax = -1;
    return;
  }

  file_des->f_ptr = filesys_open(file);
  if (file_des->f_ptr == NULL) {
    f->eax = -1;
    free(file_des);
    return;
  }

  /* Sets the file_descritpro is_dir */
  set_is_dir(file_des);

  if (file_des->is_dir)
    dir_open(file_des->f_ptr->inode);



  
  file_des->fd = thread_current()->next_fd++;
  list_push_back(&(thread_current()->file_descriptors), &(file_des->elem));
  f->eax = file_des->fd;
}

/* HELPER FUNCTION 
 * that handles the filesize routine. Returns the size of a file.
 * @fd, file descriptor
 * @f, interrupt frame 
 */
void syscall_filesize(int fd, struct intr_frame* f) {
  if (thread_current()->next_fd <= fd || fd < 0) {
    syscall_exit(-1, f);
  }
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr) {
    syscall_exit(-1, f);
  }
  f->eax = file_length(f_ptr);
}

/* HELPER FUNCTION
 * Handles the exit routine. 
 * @status, the exit status
 * @f, an interrupt frame
 */
void syscall_exit(int status, struct intr_frame* f) {
  f->eax = status;
  thread_current()->self->status = status;
  thread_exit();
}

/* HELPER FUNCTION
 * Handles the close routine.
 * Validate the passed in arguments first. Then close the file if it's open.
 * @fd, file descriptor
 */
void syscall_close(int fd, struct intr_frame* f) {
  if (thread_current()->next_fd <= fd || fd < 2) {
    syscall_exit(-1, f);
  }
  struct file_descriptor* file_descriptor_struct = get_fd_struct(fd);
  if (file_descriptor_struct == NULL) {
    syscall_exit(-1, f);
  } else {
    if (file_descriptor_struct->is_dir)
      dir_close((struct dir*)file_descriptor_struct->f_ptr);
    else
      file_close(file_descriptor_struct->f_ptr);
    list_remove(&(file_descriptor_struct->elem));
    free(file_descriptor_struct);
  }
}

/* HELPER FUNCTION
 * Handles the write function
 * @ fd, file descriptor
 * @ buffer, buffer to write to
 * @ size, size 
 * @ f, interrupt frame
 */
void syscall_write(int fd, const void* buffer, unsigned size, struct intr_frame* f) {
  struct file* f_ptr;
  if (!check_addr(buffer, size)) {
    syscall_exit(-1, f);
  }

  char* b = buffer;
  if (fd == 1) { /* Stdout case */
    putbuf(buffer, size);
    f->eax = size;
    return;
  } else {
    f_ptr = get_f_ptr(fd);
    if (f_ptr == NULL || get_fd_struct(fd)->is_dir) { /* File is not openned or is a directory */
      f->eax = -1;
      syscall_exit(-1, f);
    }
    f->eax = file_write(f_ptr, buffer, size);
  }
}


/* HELPER FUNCTION
 * Handles the read function.
 * @ fd, file descriptor
 * @ buffer, buffer to write to
 * @ size, size
 * @ f, interrupt frame
 */
void syscall_read(int fd, void* buffer, unsigned size, struct intr_frame* f) {
  if (thread_current()->next_fd <= fd || fd < 0) {
    syscall_exit(-1, f);
  }
  if (!check_addr(buffer, size)) {
    syscall_exit(-1, f);
  }
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr || fd == 1) {
    f->eax = -1;
    syscall_exit(-1, f);
  }
  if (fd == 0) { /* Stdin case */
    uint8_t c;
    int i = 0;
    char* b = (char*)buffer;
    while (size > 0) {
      c = input_getc();
      b[i] = c;
      if (c == '\r') {
        b[i] = '\n';
        f->eax = i;
        break;
      }
      size--;
      i++;
    }
  } else {
    f->eax = file_read(f_ptr, buffer, size);
  }
}

/* HELPER FUNCTION
 * Handles syscall seek routine
 * @fd, file descriptor
 * @position, position in file
 */
void syscall_seek(int fd, unsigned position, struct intr_frame* f) {
  if (thread_current()->next_fd <= fd || fd < 0) {
    syscall_exit(-1, f);
  }
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr)
    syscall_exit(-1, f);
  file_seek(f_ptr, position);
}

/* HELPER FUNCTION
 * Handles syscall tell routine
 * @fd, file descriptor
 */
void syscall_tell(int fd, struct intr_frame* f) {
  if (thread_current()->next_fd <= fd || fd < 0) {
    syscall_exit(-1, f);
  }
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr)
    syscall_exit(-1, f);
  f->eax = file_tell(f_ptr);
}

/* HELPER FUNCTION
 * Validates FD. If FD is not part of the user program
 * return FALSE and TRUE otherwise.
 */
bool valid_fd(int fd_user) {
  struct list_elem* e;
  for (e = list_begin(&thread_current()->file_descriptors);
       e != list_end(&thread_current()->file_descriptors); e = list_next(e)) {
    struct file_descriptor* f = list_entry(e, struct file_descriptor, elem);
    if (f->fd == fd_user)
      return true;
  }
  return false;
}

/* HELPER FUNCTION
 * Given a file descriptor this function returns the file associated with it or NULL
 * if the file doesn't exist.
 * @ fd, file descriptor.
 * @return file associated to the given FD.
 */
struct file* get_f_ptr(int fd) {
  struct list_elem* e;
  struct file* f_ptr = NULL;
  for (e = list_begin(&thread_current()->file_descriptors);
       e != list_end(&thread_current()->file_descriptors); e = list_next(e)) {
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);
    if (file_des->fd == fd) {
      f_ptr = file_des->f_ptr;
      return f_ptr;
    }
  }
  return NULL;
}

/* HELPER FUNCTION
 * Checks the given address if its valid or in user space
 * @ptr, pointer to check on
 * @size, size of the ptr
 * @return true for valid addr and false otherwise
 */
bool check_addr(const void* ptr, int size) {
  if (ptr == NULL) {
    return false;
  }
  if (size > 0) {
    if (!is_user_vaddr(ptr) || pagedir_get_page(thread_current()->pagedir, ptr) == NULL ||
        !is_user_vaddr(ptr + (size - 1)) ||
        pagedir_get_page(thread_current()->pagedir, ptr + (size - 1)) == NULL) {
      return false;
    }
    return true;
  } else {
    char* ptr_cpy = (char*)ptr;
    while (1) {
      if (!check_addr(ptr_cpy, 1))
        return false;
      if (*ptr_cpy == '\0')
        return true;
      ptr_cpy++;
    }
  }
}

/* HELPER FUNCTION
 * Returns the file descriptor struct pointer given the fd.
 * @fd, a file descriptor
 */
struct file_descriptor* get_fd_struct(int fd) {
  struct list_elem* e;
  for (e = list_begin(&thread_current()->file_descriptors);
       e != list_end(&thread_current()->file_descriptors); e = list_next(e)) {
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);
    if (file_des->fd == fd) {
      return file_des;
    }
  }
  return NULL;
}

/* HELPER FUNCTION
 * Handle the exec syscall routine.
 * @cmd, executable name + arguments
 */
void syscall_exec(const char* cmd, struct intr_frame* f) {
  if (!check_addr(cmd, -1))
    syscall_exit(-1, f);
  f->eax = process_execute(cmd);
}

/* HELPER FUNCTION
 * Handle the wait syscall routine.
 * @pid, child's process id to wait for.
 */
void syscall_wait(tid_t pid, struct intr_frame* f) { 
  f->eax = process_wait(pid); 
}


/* HELPER FUNCTION
 * Handle the mkdir syscall routine.
 * @path, cpath to dir + name.
 */
void syscall_mkdir(char* path, struct intr_frame* f) {
  /* It is important to set the size=2 since we will add "." and ".." */
  f->eax = filesys_create(path, 2, 1);
}


/* HELPER FUNCTION
 * Handle the chdir syscall routine.
 * @name, new cwd
 */
void syscall_chdir(char* name, struct intr_frame *f) {
  f->eax = filesys_chdir(name);
}

/* HELPER FUNCTION
 * Handle the readdir syscall routine.
 * @fd, fd of the directory
 * @name, buffer holds an entry from directory
 */
void syscall_readdir(int fd, char *name[NAME_MAX + 1], struct intr_frame *f) {
  struct file_descriptor *file_des = get_fd_struct(fd);
  if (file_des && file_des->f_ptr->inode->removed && file_des->is_dir) {
    f->eax = false;
    return;
  }
  f->eax = dir_readdir((struct dir*)file_des->f_ptr, name);
}

/* HELPER FUNCTION
 * Handle the isdir syscall routine.
 * @fd, fd of the directory
 */
void syscall_isdir(int fd, struct intr_frame *f) {
  struct file_descriptor *f_des = get_fd_struct(fd);
  if (!f_des)
    f->eax = false;
  f->eax = f_des->is_dir;
}

/* HELPER FUNCTION
 * Handle the inumber syscall routine.
 * @fd, fd of the directory/file
 */
void syscall_inumber(int fd, struct intr_frame *f) {
  struct file_descriptor *f_des = get_fd_struct(fd);
  if (!f_des)
    f->eax = false;
  f->eax = inode_get_inumber(f_des->f_ptr->inode);
}





