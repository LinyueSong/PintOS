#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/console.h"

static void syscall_handler(struct intr_frame*);
void syscall_create(const char* file, unsigned initial size, struct intr_frame* f);
void syscall_remove(const char* file, struct intr_frame* f);
void syscall_open(const char* file, struct intr_frame* f);
void syscall_filesize(int fd, struct intr_frame* f);
void syscall_read(int fd, void* buffer, unsigned size, struct intr_frame* f);
void syscall_write(int fd, const void* buffer, unsigned size, struct intr_frame* f);
void syscall_seek(int fd, unsigned position);
void syscall_tell(int fd, struct intr_frame* f);
void syscall_close(int fd);
void syscall_exit(struct intr_frame* f);
bool valid_fd(int fd_user);
file* get_f_ptr(int fd);

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

  /* VIRGIL:
   * before calling a syscall helper we must make sure that the arguments passed
   * to us are valid.
   */

  switch (args[0]) {
    case SYS_CREATE:
      syscall_create(args[1], args[2], &f);
      break;
    case SYS_REMOVE:
      syscall_remove(args[1], &f);
      break;
    case SYS_OPEN:
      syscall_open(args[1], &f);
      break;
    case SYS_FILESIZE:
      syscall_filesize(args[1], &f);
      break;
    case SYS_READ:
      syscall_read(args[1], args[2], args[3] & f);
      break;
    case SYS_WRITE:
      syscall_write(args[1], args[2], args[3], &f);
      break;
    case SYS_SEEK:
      syscall_seek(args[1], args[2]);
      break;
    case SYS_TELL:
      syscall_tell(args[1], &f);
      break;
    case SYS_CLOSE:
      syscall_close(args[1]);
      break;
    case SYS_EXIT:
      syscall_exit(&f);
      break;
    default:
      /* PANIC? */
      syscall_exit(&f); // temporary, need a different strategy (we don't want kill the process)
  }
}

/* design doc changes
 * We have move the exit routine in different syscall_exit and incorporate it
 * in our switch statement. In addition to moving it we will modify it as well.
 * On a exit we will implicitly close all the files descriptors as spec requires.
 *
 *
 *
 *
 *
 *
 *
 *
 * /


/* HELPER FUNCTION 
 * that handles the create routine. Creates a file of some size.
 * @file, name of the file
 * @size, size of the file
 * @f, interrupt frame 
 */

void syscall_create(const char* file, unsigned initial size, struct intr_frame* f) {
  check_addr(file, size);
  lock_acquire(*filesys_lock);
  f->eax = filesys_create(file, size);
  lock_release(*filesys_lock);
}

/* HELPER FUNCTION 
 * that handles the remove routine. Removes a file.
 * @file, name of the file
 * @f, interrupt frame 
 */

void syscall_remove(const char* file, struct intr_frame* f) {
  lock_acquire(*filesys_lock);
  f->eax = filesys_remove(file);
  lock_release(*filesys_lock);
}

/* HELPER FUNCTION 
 * that handles the open routine. Opens a file.
 * @file, name of the file
 * @f, interrupt frame 
 */

void syscall_open(const char* file, struct intr_frame* f) {
  lock_acquire(*filesys_lock);
  struct file_descriptor* file_des = malloc(sizeof(file_descriptor));
  file_des->f_ptr = filesys_open(file);
  if (f_ptr == NULL) {
    f->eax = -1 syscall_exit();
  }
  file_des->fd = thread_current()->next_fd++;
  list_push_back(file_descriptors, &(file_des->elem));
  f->eax = file_des->fd;
  lock_release(*filesys_lock);
}

/* HELPER FUNCTION 
 * that handles the filesize routine. Returns the size of a file.
 * @fd, file descriptor
 * @f, interrupt frame 
 */

void syscall_filesize(int fd, struct intr_frame* f) {
  if (thread_current()->next_fd >= fd || fd < 0) {
    syscall_exit();
  }
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr) {
    syscall_exit();
  }
  lock_acquire(*filesys_lock);
  f->eax = file_length(f_ptr);
  lock_release(*filesys_lock);
}

/* HELPER FUNCTION
 * Handles the exit routine. Implicitly closes all its open file descriptors.
 * @f, an interrupt frame
 *
 *------What about 0 and 1 should we close it or pintos close them implicitly
 *------When we exit how do we know, print status. Why we need that? If we
 *      call exit from different functions would be nice to know what is the exit status.
 *
 */
void syscall_exit(struct intr_frame* f) {
  f->eax = args[1];
  struct list_elem* e;

  for (e = list_begin(&file_descriptors); e != list_end(&file_descriptors); e = list_next(e)) {
    struct file_descriptor* fd = list_entry(e, struct file_descriptor, elem);
    syscall_close(fd->fd);
  }
  printf("%s: exit(%d)\n", &thread_current()->name, args[1]);
  thread_exit();
}

/* HELPER FUNCTION
 * Handles the close routine.
 * Check first if the file descriptor is valid
 * @fd, file descriptor
 */
void syscall_close(int fd) {
  if (current_thread()->next_fd >= fd || fd < 2) {
    syscall_exit();
  }
  lock_acquire(*filesys_lock);
  file* file_user = get_f_ptr(fd);
  if (file_user == NULL) {
    syscall_exit();
  } else {
    file_close(file_user);
    // need to free the struct
  }
  lock_acquire(*filesys_lock);
}

/* HELPER FUNCTION
 * Handles the write function
 * @ fd, file descriptor
 * @ buffer, buffer to write to
 * @ size, size 
 * @ f, interrupt frame
 *
 * -------- We need also to check if we don't go out of bounds when we write on a file.
 */
void syscall_write(int fd, const void* buffer, unsigned size, struct intr_frame* f) {
  if (thread_current()->next_fd >= fd || fd < 0) {
    syscall_exit();
  }
  check_addr(buffer, size);
  struct list_elem* e;
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr)
    syscall_exit();
  lock_acquire(*filesys_lock);
  if (fd == 1) {
    if (size > 200) {
      int i = 200;
      do {
        f->eax = putbuf(f_ptr, buffer, i);
        size -= i;
        if (size < 200) {
          i = size;
        }
      } while (size > 0)
    } else {
      f->eax = putbuf(f_ptr, buffer, size);
    }
  } else {
    f->eax = file_write(f_ptr, buffer, size);
  }
  lock_release(*filesys_lock);
}

/* HELPER FUNCTION
 * @ fd, file descriptor
 * @ buffer, buffer to write to
 * @ size, size
 * @ f, interrupt frame
 *
 *
 * 
 */

void syscall_read(int fd, void* buffer, unsigned size, struct intr_frame* f) {
  if (thread_current()->next_fd >= fd || fd < 0) {
    syscall_exit();
  }
  check_addr(buffer, size);
  struct list_elem* e;
  struct file* f_ptr;
  for (e = list_begin(&file_descriptors); e != list_end(&file_descriptors); e = list_next(e)) {
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);
    if (file_des->fd == fd) {
      f_ptr = file_des->f_ptr;
      break;
    }
  }
  lock_acquire(*filesys_lock);
  f->eax = file_read(f_ptr, buffer, size);
  lock_release(*filesys_lock);
}

void syscall_seek(int fd, unsigned position) {
  if (thread_current()->next_fd >= fd || fd < 0) {
    syscall_exit();
  }
  struct list_elem* e;
  struct file* f_ptr;
  for (e = list_begin(&file_descriptors); e != list_end(&file_descriptors); e = list_next(e)) {
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);
    if (file_des->fd == fd) {
      f_ptr = file_des->f_ptr;
      break;
    }
  }
  lock_acquire(*filesys_lock);
  f->eax = file_seek(f_ptr, position);
  lock_release(*filesys_lock);
}

void syscall_tell(int fd, struct intr_frame* f) {
  if (thread_current()->next_fd >= fd || fd < 0) {
    syscall_exit();
  }
  struct file* f_ptr = get_f_ptr(fd);
  if (!f_ptr)
    syscall_exit();
  lock_acquire(*filesys_lock);
  f->eax = file_tell(f_ptr);
  lock_release(*filesys_lock);
}

/* HELPER FUNCTION
 * Validates FD. If FD is not part of the user program
 * return FALSE and TRUE otherwise.
 */
bool valid_fd(int fd_user) {
  struct list_elem* e;

  for (e = list_begin(&file_descriptors); e != list_end(&file_descriptors); e = list_next(e)) {
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
file* get_f_ptr(int fd) {
  struct list_elem* e;
  struct file* f_ptr = NULL;
  for (e = list_begin(&file_descriptors); e != list_end(&file_descriptors); e = list_next(e)) {
    struct file_descriptor* file_des = list_entry(e, struct file_descriptor, elem);
    if (file_des->fd == fd) {
      f_ptr = file_des->f_ptr;
      return f_ptr;
    }
  }
}
