/* Test if seek changes the offset of the file */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  int handle = open("sample.txt");
  if (handle < 2) fail("open() returned %d", handle);
  seek(handle, 5);
  char buff[1];
  read(handle, &buff, 1);
  if (buff[0] != 'i') fail("seek returned %d", buff[0]);
}
