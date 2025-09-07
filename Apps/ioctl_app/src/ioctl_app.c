#include "ioctl_header.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main() {
  int fd;
  int ret;
  int read_data;
  char write_data[] = "Hello from user space!";
  int write_size = strlen(write_data);

  // 打开设备文件
  fd = open("/dev/mydevice", O_RDWR);
  if (fd < 0) {
    perror("Failed to open device");
    return -1;
  }

  printf("--- Testing TEST_NODATA ---\n");
  ret = ioctl(fd, TEST_NODATA);
  if (ret == 0) {
    printf("TEST_NODATA successful.\n");
  } else {
    printf("TEST_NODATA failed.\n");
  }

  printf("\n--- Testing TEST_READ ---\n");
  // 将read_data的地址传递给驱动
  ret = ioctl(fd, TEST_READ, &read_data);
  if (ret == 0) {
    printf("TEST_READ successful. Data read from kernel: %d\n", read_data);
  } else {
    printf("TEST_READ failed.\n");
  }

  printf("\n--- Testing TEST_WRITE ---\n");
  // 将write_data的地址传递给驱动
  ret = ioctl(fd, TEST_WRITE, write_data);
  if (ret == 0) {
    printf("TEST_WRITE successful. Data sent: '%s'\n", write_data);
  } else {
    printf("TEST_WRITE failed.\n");
  }

  close(fd);
  return 0;
}