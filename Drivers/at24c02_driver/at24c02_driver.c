/***************************************************************
文件名     : at24c02_driver.c
作者       : ccoisini
描述       : dht11 驱动程序, GPIO4_PIN19-----DHT11 IO port

使用方法：
在.dts文件中定义节点信息

  &i2c1 {
    clock-frequency = <100000>;
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_i2c1>;
    status = "okay";

    at24c02: at24c02@50{
      compatible = "ccoisini,at24c02";
      reg = <0x50>;
    };
  };

***************************************************************/

#include "at24c02_header.h"

// 全局的私有数据指针，通常在probe中分配并赋值给i2c_client的私有数据
static struct at24c02_struct *g_at24c02_data;

static int at24c02_open(struct inode *inode, struct file *file)
{
    struct at24c02_struct *data = container_of(inode->i_cdev, struct at24c02_struct, cdev);
    file->private_data = data;
    pr_info("at24c02 device opened.\n");
    return 0;
}

static int at24c02_release(struct inode *inode, struct file *file)
{
    pr_info("at24c02 device closed.\n");
    return 0;
}

static long at24c02_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct at24c02_struct *my_data = file->private_data;
    struct i2c_client *client = my_data->client;
    struct at24c02_io_data data;
    // 定义两个 I2C 消息，用于组合传输
    struct i2c_msg msgs[2];
    u8 *write_buf = NULL;
    struct i2c_msg msg;

    // 1. 从用户空间复制 ioctl 的参数结构体
    // 这步是必需的，它将用户传入的结构体内容复制到内核栈上
    if (copy_from_user(&data, (struct at24c02_io_data __user *)arg, sizeof(data)))
    {
        return -EFAULT;
    }

    // 2. 验证参数
    // 确保地址和长度都在EEPROM的有效范围内 (AT24C02容量为256字节)
    if (data.address + data.len > 256)
    {
        pr_err("Invalid address or length, exceeds device capacity.\n");
        return -EINVAL;
    }

    switch (cmd)
    {
    case AT24C02_RANDOM_READ:
    {
        // 消息1: 设置要读取的地址 (写操作)
        msgs[0].addr = client->addr;
        msgs[0].flags = 0; // 写操作标志
        msgs[0].len = 1;
        msgs[0].buf = (u8 *)&data.address;

        // 消息2: 从该地址读取数据 (读操作)
        msgs[1].addr = client->addr;
        msgs[1].flags = I2C_M_RD; // 读操作标志
        msgs[1].len = data.len;

        // 为读操作分配内核缓冲区
        msgs[1].buf = kmalloc(data.len, GFP_KERNEL);
        if (!msgs[1].buf)
        {
            return -ENOMEM;
        }

        // 执行组合传输
        ret = i2c_transfer(client->adapter, msgs, 2);
        if (ret != 2)
        {
            pr_err("I2C random read failed: %d\n", ret);
            ret = -EIO;
        }
        else
        {
            // 成功后，将数据复制回用户空间
            if (copy_to_user(data.buf, msgs[1].buf, data.len))
            {
                ret = -EFAULT;
            }
        }

        kfree(msgs[1].buf);
        return ret;
    }
    case AT24C02_BYTE_WRITE:
    {
        // 为写操作分配内核缓冲区，大小为地址（1字节）+ 数据长度
        write_buf = kmalloc(data.len + 1, GFP_KERNEL);
        if (!write_buf)
        {
            return -ENOMEM;
        }

        // 第一个字节是EEPROM内部地址
        write_buf[0] = (u8)data.address;

        // 从用户空间复制数据到内核缓冲区
        if (copy_from_user(&write_buf[1], data.buf, data.len))
        {
            kfree(write_buf);
            return -EFAULT;
        }

        // 准备 I2C 消息
        msg.addr = client->addr;
        msg.flags = 0; // 写操作
        msg.len = data.len + 1;
        msg.buf = write_buf;

        // 执行 I2C 传输
        ret = i2c_transfer(client->adapter, &msg, 1);
        if (ret != 1)
        { // 期望1个消息成功
            pr_err("I2C byte write failed: %d\n", ret);
            ret = -EIO;
        }

        kfree(write_buf);
        // Must have 20ms delay for writing
        mdelay(20);
        return ret;
    }
    default:
        return -ENOTTY; // 无效命令
    }
}

static const struct file_operations at24c02_fops = {
    .owner = THIS_MODULE, .open = at24c02_open, .release = at24c02_release, .unlocked_ioctl = at24c02_ioctl};

int at24c02_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct at24c02_struct *my_data;

    pr_info("at24c02_probe: Found new I2C device at address 0x%x\n", client->addr);

    // 1. 分配私有数据结构
    my_data = devm_kzalloc(&client->dev, sizeof(struct at24c02_struct), GFP_KERNEL);
    if (!my_data)
    {
        pr_err("Failed to allocate private data for device\n");
        return -ENOMEM;
    }
    g_at24c02_data = my_data;

    // 2. 将I2C客户端指针保存到私有数据中
    my_data->client = client;

    // 3. 将私有数据附加到客户端上，这样可以在任何地方通过client->dev->driver_data获取
    i2c_set_clientdata(client, my_data);

    // 4. 动态分配设备号
    ret = alloc_chrdev_region(&my_data->dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("Failed to allocate major number\n");
        goto err_out; // 托管资源会自动清理
    }

    // 5. 初始化并添加字符设备
    cdev_init(&my_data->cdev, &at24c02_fops);
    my_data->cdev.owner = THIS_MODULE;
    ret = cdev_add(&my_data->cdev, my_data->dev_number, 1);
    if (ret < 0)
    {
        pr_err("Failed to add character device\n");
        goto err_unregister_dev;
    }

    // 6. 创建设备类
    my_data->class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_data->class))
    {
        pr_err("Failed to create device class\n");
        ret = PTR_ERR(my_data->class);
        goto err_cdev_del;
    }

    // 7. 创建设备节点
    my_data->device = device_create(my_data->class, &client->dev, my_data->dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(my_data->device))
    {
        pr_err("Failed to create device node\n");
        ret = PTR_ERR(my_data->device);
        goto err_class_destroy;
    }

    pr_info("at24c02 probe success. Device node created at /dev/%s\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(my_data->class);
err_cdev_del:
    cdev_del(&my_data->cdev);
err_unregister_dev:
    unregister_chrdev_region(my_data->dev_number, 1);
err_out:
    // devm_kzalloc 自动清理，这里无需kfree
    pr_err("at24c02_probe failed\n");
    return ret;
}

int at24c02_remove(struct i2c_client *client)
{
    // 1. 获取私有数据指针
    struct at24c02_struct *my_data = i2c_get_clientdata(client);

    pr_info("at24c02_remove: Removing device at address 0x%x\n", client->addr);

    // 2. 销毁设备节点
    device_destroy(my_data->class, my_data->dev_number);

    // 3. 销毁设备类
    class_destroy(my_data->class);

    // 4. 删除字符设备
    cdev_del(&my_data->cdev);

    // 5. 注销设备号
    unregister_chrdev_region(my_data->dev_number, 1);

    // 6. 托管资源会自动释放，这里无需kfree

    pr_info("at24c02_remove success.\n");
    return 0;
}

// 解决modpost错误，同时支持设备树和传统匹配，这是一种更健壮的方式
static const struct i2c_device_id at24c02_id_table[] = {{COMPATIBLE_NAME, 0}, {}};
MODULE_DEVICE_TABLE(i2c, at24c02_id_table);

static const struct of_device_id at24c02_of_match[] = {{.compatible = COMPATIBLE_NAME}, {/* 哨兵 */}};

static struct i2c_driver at24c02_driver = {
    .driver = {.name = KBUILD_MODNAME, .owner = THIS_MODULE, .of_match_table = at24c02_of_match},
    .probe = at24c02_probe,
    .remove = at24c02_remove,
    // 如果不使用设备树，那么再自行添加.id_table
    .id_table = at24c02_id_table,
};

module_i2c_driver(at24c02_driver);
MODULE_AUTHOR("CCoisini");
MODULE_DESCRIPTION("This is a test driver");
MODULE_LICENSE("GPL");