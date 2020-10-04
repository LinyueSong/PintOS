/* Test if seek changes the offset of the file */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
void test_main(void) {
  int handle = open("sample.txt");
  if (handle < 2) fail("open() returned %d", handle);
  seek(handle, 1000);
  char buff[1];
  int bytes_read = read(handle, &buff, 1);
  if (bytes_read != 0) fail("seek returned %d", buff[0]);
}
