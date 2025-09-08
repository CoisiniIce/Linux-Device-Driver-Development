#include "at24c02_header.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#define AT24C02_FILE_PATH "/dev/at24c02_device"

int main(int argc, char **argv) {
  int fd, ret;
  struct at24c02_io_data io_data;
  // AT24C02 的页大小是 8 字节。这意味着它在一次写操作中最多只能写入 8
  // 个字节。如果试图一次性写入超过 8 字节的数据，EEPROM
  // 的内部地址指针会自动回绕。 驱动层没有处理绕回，所以此处只能先用8字节测试
  char write_buf[] = "abcdefg";
  char read_buf[30]; // 缓冲区大小要足够大

  // 1. 打开设备节点
  printf("Opening device: at24c02_device\n");
  fd = open(AT24C02_FILE_PATH, O_RDWR);
  if (fd < 0) {
    perror("Failed to open the device\n");
    return -1;
  }
  printf("Device opened successfully.\n");

  // 2. 写入数据
  memset(read_buf, 0, sizeof(read_buf)); // 清零读缓冲区

  printf("\n--- Writing data to EEPROM ---\n");
  io_data.address = 0x10; // EEPROM 内部地址
  io_data.len = strlen(write_buf);
  io_data.buf = (unsigned char *)write_buf;

  printf("Writing %lu bytes to address 0x%x...\n", io_data.len,
         io_data.address);
  ret = ioctl(fd, AT24C02_BYTE_WRITE, &io_data);
  if (ret < 0) {
    perror("ioctl write failed\n");
    close(fd);
    return -1;
  }
  printf("Write successful.\n");

  // 3. 读取数据
  printf("\n--- Reading data from EEPROM ---\n");
  io_data.address = 0x10;          // 从刚才写入的地址开始读
  io_data.len = strlen(write_buf); // 读回相同的长度
  io_data.buf = (unsigned char *)read_buf;

  printf("Reading %d bytes from address 0x%x...\n", io_data.len,
         io_data.address);
  ret = ioctl(fd, AT24C02_RANDOM_READ, &io_data);
  if (ret < 0) {
    perror("ioctl read failed\n");
    close(fd);
    return -1;
  }
  printf("Read successful.\n");

  // 4. 打印并验证数据
  printf("Data read back: %s\n", read_buf);

  if (strcmp(write_buf, read_buf) == 0) {
    printf("Verification successful: The data matches!\n");
  } else {
    printf("Verification failed: The data does NOT match.\n");
  }

  // 5. 关闭设备
  close(fd);
  printf("\nDevice closed.\n");
  return 0;
}