#include "asm/uaccess.h"
#include "ioctl_header.h"
#include "linux/printk.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>

#define DEVICE_NAME "mydevice"
#define CLASS_NAME "myclass"

static int major;
static struct class *myclass = NULL;
static struct cdev mycdev;

static int mydevice_open(struct inode *inode, struct file *file)
{
    // 打开设备实现
    return 0;
}

static int mydevice_release(struct inode *inode, struct file *file)
{
    // 关闭设备实现
    return 0;
}

static ssize_t mydevice_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    // 读取设备实现
    return 0;
}

static ssize_t mydevice_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    // 写入设备实现
    return len;
}

static long mydevice_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int size = 123;
    char my_write_data[TEST_MAX_WRITE]; 
    switch (cmd)
    {
    case TEST_NODATA:
        pr_info("Nodata triggered\n");
        break;
    case TEST_READ:
        pr_info("Read triggered\n");
        // 从内核空间复制数据到用户空间
        if (copy_to_user((int __user *)arg, &size, sizeof(size))) {
            return -EFAULT; // 复制失败返回错误码
        }
        break;
    case TEST_WRITE:
        pr_info("Write triggered\n");
        // 从用户空间复制数据到内核空间
        if (copy_from_user(my_write_data, (const void __user *)arg, TEST_MAX_WRITE)) {
            return -EFAULT; // 复制失败返回错误码
        }
        pr_info("Received data: %s", my_write_data);
        break;
    default:
        pr_info("Don't have this cmd\n");
        break;
    }
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = mydevice_open,
    .release = mydevice_release,
    .read = mydevice_read,
    .write = mydevice_write,
    .unlocked_ioctl = mydevice_ioctl,
};

static int __init mydevice_init(void)
{
    dev_t dev = 0;

    // 1. 分配设备号
    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0)
    {
        return -1;
    }
    major = MAJOR(dev);

    // 2. 初始化并添加字符设备
    cdev_init(&mycdev, &fops);
    mycdev.owner = THIS_MODULE;
    if (cdev_add(&mycdev, dev, 1) < 0)
    {
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    // 3. 创建设备类
    myclass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(myclass))
    {
        cdev_del(&mycdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(myclass);
    }

    // 4. 创建设备节点
    device_create(myclass, NULL, dev, NULL, DEVICE_NAME);

    printk(KERN_INFO "My device driver loaded\n");
    return 0;
}

static void __exit mydevice_exit(void)
{
    dev_t dev = MKDEV(major, 0);

    // 1. 销毁设备节点
    device_destroy(myclass, dev);

    // 2. 销毁设备类
    class_destroy(myclass);

    // 3. 删除字符设备
    cdev_del(&mycdev);

    // 4. 释放设备号
    unregister_chrdev_region(dev, 1);

    printk(KERN_INFO "My device driver unloaded\n");
}

module_init(mydevice_init);
module_exit(mydevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux device driver");