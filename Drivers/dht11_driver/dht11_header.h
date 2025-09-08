#ifndef __DHT11_HEADER_H__
#define __DHT11_HEADER_H__

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "dht11_device"
#define CLASS_NAME "dht11_class"
#define COMPATIBLE_NAME "ccoisini,dht11"

struct dht11_struct
{
    dev_t dev_number;
    struct class *class;
    struct cdev cdev;
    struct device *device;
    struct gpio_desc *pin;
};

#endif