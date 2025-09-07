#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main(int argc, char **argv) {
  int fd;
  fd_set readfds, writefds;

  // 打开你的设备文件
  fd = open("/dev/mydevice", O_RDWR);
  if (fd < 0) {
    perror("Failed to open /dev/mydevice");
    return -1;
  }

  // 设置文件描述符集合
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(fd, &readfds);
  FD_SET(fd, &writefds);

  ioctl(int fd, unsigned long request, ...)

  printf("Waiting for device to be ready to read or write...\n");

    // 设置超时时间为永不超时 (NULL)
    int ret = select(fd + 1, &readfds, &writefds, NULL, NULL);

    if (ret == -1) {
        perror("select");
    } else if (ret) {
        // select 成功返回，检查是哪个事件
        if (FD_ISSET(fd, &readfds)) {
            printf("Device is ready to read!\n");
        }
        if (FD_ISSET(fd, &writefds)) {
            printf("Device is ready to write!\n");
        }
    }

    close(fd);
    return 0;
}