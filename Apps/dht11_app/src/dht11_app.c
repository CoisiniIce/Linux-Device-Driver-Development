#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DHT11_FILE_PATH "/dev/dht11_device"

int main(int argc, char **argv) {
  int fd;
  unsigned char dht11_data_buffer[5];
  fd = open(DHT11_FILE_PATH, O_RDONLY);
  if (fd < 0) {
    printf("Open failed\r\n");
    return -1;
  }
  for (;;) {
    int read_len = sizeof(dht11_data_buffer);
    if (read(fd, dht11_data_buffer, read_len) == read_len) {
      printf("湿度: %d.%d\n", dht11_data_buffer[0], dht11_data_buffer[1]);
      printf("温度: %d.%d\n", dht11_data_buffer[2], dht11_data_buffer[3]);
    } else {
      perror("Read DHT11 data failed\r\n");
    }
    sleep(1);
  }
  close(fd);

  return 0;
}