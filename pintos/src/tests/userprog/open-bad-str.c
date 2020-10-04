/* Test if the open syscall checks the string is null terminated */
#include <syscall-nr.h>
#include "tests/userprog/boundary.h"
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  char* p = get_bad_boundary();
  p -= 10;
  char file_name[] = {'s','a','m','p','l','e','.','t','x','t'};
  for (int i = 0; i < 10; i++) {
    *p = file_name[i];
    p++;
  }
  int handle = open(p-10);
  fail("didn't validate file name");
}
