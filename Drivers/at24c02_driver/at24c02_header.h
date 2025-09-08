#ifndef __AT24C02_HEADER_H__
#define __AT24C02_HEADER_H__

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "at24c02_device"
#define CLASS_NAME "at24c02_class"
#define COMPATIBLE_NAME "ccoisini,at24c02"

#define AT24C02_MAGIC 'E'

// 定义一个通用的数据结构，用于在 ioctl 中传递地址、长度和数据
struct at24c02_io_data
{
    __u16 address;    // EEPROM 内部的起始地址
    __u16 len;        // 读或写的字节数
    __u8 __user *buf; // 指向用户空间数据缓冲区的指针
};

#define AT24C02_RANDOM_READ _IOWR(AT24C02_MAGIC, 1, struct at24c02_io_data)
#define AT24C02_BYTE_WRITE _IOW(AT24C02_MAGIC, 2, struct at24c02_io_data)

struct at24c02_struct
{
    dev_t dev_number;
    struct class *class;
    struct cdev cdev;
    struct device *device;
    struct i2c_client *client;
};

#endif