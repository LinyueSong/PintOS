#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include <random.h>

char buf1[1];

void test_main(void) {
  int fd;
  create("test.txt", sizeof(buf1));
  random_bytes(buf1, sizeof(buf1));
  fd = open("test.txt");
  for (int i = 0; i < 128 * 512; i++) {
    write(fd, buf1, 1);
  }
  for (int i = 0; i < 128 * 512; i++) {
    read(fd, buf1, 1);
  }
  close(fd);
  unsigned long long wcnt = get_block_wcnt();
  if (wcnt % 128 == 0) {
    msg ("success");
  }
}