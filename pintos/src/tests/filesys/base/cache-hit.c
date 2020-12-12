#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

char buf1[512 * 2];

void test_main(void) {
  int fd;
  create("cache_hit_test_file.txt", sizeof(buf1));
  memset(buf1, 2, 400);
  fd = open("cache_hit_test_file.txt");
  write(fd, buf1, sizeof(buf1));
  close(fd);
  flush_cache();
  fd = open("cache_hit_test_file.txt");
  read(fd, buf1, sizeof(buf1));
  close(fd);
  int cold_hit_rate = hit_rate();
  fd = open("cache_hit_test_file.txt");
  read(fd, buf1, sizeof(buf1));
  close(fd);
  int hot_hit_rate = hit_rate();
  msg(cold_hit_rate > hot_hit_rate);
}