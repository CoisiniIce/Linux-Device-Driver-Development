#include "linux/printk.h"
#include "linux/wait.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>

#define DEVICE_NAME "mydevice"
#define CLASS_NAME "myclass"

int major;
struct class *myclass;
struct cdev mycdev;
static int ready_to_read = 0;
static int ready_to_write = 0;

static DECLARE_WAIT_QUEUE_HEAD(my_wq);
static DECLARE_WAIT_QUEUE_HEAD(my_rq);

static int mydevice_open(struct inode *inode, struct file *file)
{
    // 打开设备实现
    pr_info("Open is triggered\n");
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
    // 假设写入操作使设备变为可读
    ready_to_read = 1;
    pr_info("Data written, waking up readers...\n");
    // 唤醒 my_rq 等待队列上的所有进程
    wake_up_interruptible(&my_rq);
    return len;
}

static unsigned int mydevice_poll(struct file *file, struct poll_table_struct *wait)
{
    unsigned int reval_mask = 0;
    poll_wait(file, &my_wq, wait);
    poll_wait(file, &my_rq, wait);

    if (ready_to_read)
    {
        reval_mask |= (POLLIN | POLLRDNORM);
    }
    else if (ready_to_write)
    {
        reval_mask |= (POLLOUT | POLLWRNORM);
    }
    return reval_mask;
}

static struct file_operations fops = {.owner = THIS_MODULE,
                                      .open = mydevice_open,
                                      .release = mydevice_release,
                                      .read = mydevice_read,
                                      .write = mydevice_write,
                                      .poll = mydevice_poll};

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

    pr_info("My device driver loaded\n");
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

    pr_info("My device driver unloaded\n");
}

module_init(mydevice_init);
module_exit(mydevice_exit);
MODULE_AUTHOR("CCoisini <WJH_ls@outlook.com>");
MODULE_LICENSE("GPL");