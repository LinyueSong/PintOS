#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include <random.h>

char buf1[512 * 2];

void test_main(void) {
  int fd;
  create("test.txt", sizeof(buf1));
  random_bytes(buf1, sizeof(buf1));
  fd = open("test.txt");
  write(fd, buf1, sizeof(buf1));
  close(fd);
  flush_cache();
  fd = open("test.txt");
  read(fd, buf1, sizeof(buf1));
  close(fd);
  int cold_hits = hit_rate();
  fd = open("test.txt");
  read(fd, buf1, sizeof(buf1));
  close(fd);
  int hot_hits = hit_rate();
  if (cold_hits < hot_hits) {
    msg("success");
  }
}
