/* Test if the tell syscall correctly returns the file offset after seek syscalls */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  int handle = open("sample.txt");
  if (handle < 2) fail("open() returned %d", handle);
  seek(handle, 5);
  int position = tell(handle);
  if (position != 5) fail("tell() returned %d", position);
}
