/***************************************************************
文件名     : dht11_driver.c
作者       : ccoisini
描述       : dht11 驱动程序, GPIO4_PIN19-----DHT11 IO port

使用方法：
1) 在.dts文件中定义节点信息
    // IO: GPIO-4-PIN19
    dht11 {
        compatible = "ccoisini,dht11";
        pinctrl-names = "default";
        pinctrl-0 = <&dht11_pins>;
        // 0 表示通用GPIO，不指定有效电平
        gpios = <&gpio4 19 0>;
        status = "okay";
    };
2)  在＆iomuxc的imx6ul-evk下添加
    dht11_pins: dht11grp{
        fsl,pins = <
        MX6UL_PAD_CSI_VSYNC__GPIO4_IO19            0x000010B0
        >;
    };
***************************************************************/

#include "dht11_header.h"

// int us_low_array[40];
// int us_low_index;
// int us_array[40];
// int time_array[40];
// int us_index;

static void dht11_release(struct dht11_struct *my_data) { gpiod_direction_output(my_data->pin, 1); }

static void dht11_start(struct dht11_struct *my_data)
{
    gpiod_direction_output(my_data->pin, 1);
    mdelay(30);

    gpiod_set_value(my_data->pin, 0);
    mdelay(20);

    gpiod_set_value(my_data->pin, 1);
    udelay(40);

    gpiod_direction_input(my_data->pin);
}

static int dht11_wait_ack(struct dht11_struct *my_data)
{
    int timeout_us;
    int ret = 0;

    /* 1. 等待 DHT11 的低电平应答信号 (80us) */
    timeout_us = 100; // 留出余量，协议规定应答信号为 80us
    while (gpiod_get_value(my_data->pin) && --timeout_us)
    {
        udelay(1);
    }
    if (!timeout_us)
    {
        ret = -ETIMEDOUT;
        pr_err("DHT11 ACK timeout: waiting for low level.\n");
        goto fail;
    }

    /* 2. 等待 DHT11 的高电平应答信号 (80us) */
    timeout_us = 100;
    while (!gpiod_get_value(my_data->pin) && --timeout_us)
    {
        udelay(1);
    }
    if (!timeout_us)
    {
        ret = -ETIMEDOUT;
        pr_err("DHT11 ACK timeout: waiting for high level.\n");
        goto fail;
    }

    /* 3. 等待 DHT11 释放总线 */
    timeout_us = 100;
    while (gpiod_get_value(my_data->pin) && --timeout_us)
    {
        udelay(1);
    }
    if (!timeout_us)
    {
        ret = -ETIMEDOUT;
        pr_err("DHT11 ACK timeout: waiting for bus release.\n");
        goto fail;
    }

    // 成功，返回 0
    return 0;

fail:
    // 单一出口点，返回错误码
    return ret;
}

static int dht11_read_byte(struct dht11_struct *my_data, unsigned char *datalist)
{
    int i;
    unsigned int data = 0;
    int timeout_us;
    ktime_t pre_time;
    ktime_t post_time;
    s64 duration_ns;

    // 等待开始信号的高电平部分（50us）结束
    timeout_us = 100;
    while (gpiod_get_value(my_data->pin) && --timeout_us)
    {
        udelay(1);
    }
    if (!timeout_us)
    {
        pr_err("DHT11 read byte timeout: initial low level.\n");
        return -ETIMEDOUT;
    }

    // 循环读取 8 位数据
    for (i = 0; i < 8; i++)
    {
        // 等待数据信号的低电平部分（50us）结束
        timeout_us = 100;
        while (!gpiod_get_value(my_data->pin) && --timeout_us)
        {
            udelay(1);
        }
        if (!timeout_us)
        {
            pr_err("DHT11 read byte timeout: waiting for bit %d high level.\n", i);
            return -ETIMEDOUT;
        }

        // 测量高电平持续时间
        pre_time = ktime_get();
        timeout_us = 100;
        while (gpiod_get_value(my_data->pin) && --timeout_us)
        {
            udelay(1);
        }
        post_time = ktime_get();

        if (!timeout_us)
        {
            pr_err("DHT11 read byte timeout: waiting for bit %d low level.\n", i);
            return -ETIMEDOUT;
        }

        // 根据高电平持续时间判断位值
        // DHT11 协议: 26-28us 高电平为 '0', 70us 高电平为 '1'
        duration_ns = ktime_to_ns(ktime_sub(post_time, pre_time));

        if (duration_ns > 45000)
        { // 超过 45us，判定为 1
            data = (data << 1) | 1;
        }
        else
        { // 否则，判定为 0
            data = (data << 1) | 0;
        }
    }

    *datalist = data;
    return 0;
}
static int dht11_get_data(struct dht11_struct *my_data, unsigned char *dht11_data_buffer)
{
    unsigned long flags;
    int i;
    int ret = 0; // 使用一个变量来统一管理返回值

    local_irq_save(flags);

    /* 1. 发送高脉冲启动DHT11 */
    dht11_start(my_data);

    /* 2. 等待DHT11就绪 */
    ret = dht11_wait_ack(my_data);
    if (ret)
    {
        pr_err("DHT11 ACK failed.\n");
        // 使用 -EIO 表示 I/O 错误，比 -EAGAIN 更精确
        ret = -EIO;
        goto restore_irq;
    }

    /* 3. 读 5 字节数据 */
    for (i = 0; i < 5; i++)
    {
        ret = dht11_read_byte(my_data, &dht11_data_buffer[i]);
        if (ret)
        {
            pr_err("Failed to read byte %d from DHT11.\n", i);
            ret = -EIO;
            goto restore_irq;
        }
    }

    /* 4. 释放总线 */
    dht11_release(my_data);

    /* 5. 根据校验码验证数据 */
    if (dht11_data_buffer[4] !=
        (dht11_data_buffer[0] + dht11_data_buffer[1] + dht11_data_buffer[2] + dht11_data_buffer[3]))
    {
        pr_err("DHT11 checksum mismatch.\n");
        ret = -EIO;
    }

restore_irq:
    local_irq_restore(flags);
    return ret;
}

int dht11_open(struct inode *inode, struct file *file)
{
    // 进行一些初始化
    struct dht11_struct *my_data = container_of(inode->i_cdev, struct dht11_struct, cdev);
    file->private_data = my_data;
    return 0;
}

int dht11_close(struct inode *inode, struct file *file) { return 0; }

ssize_t dht11_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    int ret;
    unsigned char dht11_data_buffer[5];
    struct dht11_struct *my_data = file->private_data;

    ret = dht11_get_data(my_data, dht11_data_buffer);
    // if get data failed
    if (ret)
    {
        return ret;
    }

    // 检查用户提供的缓冲区大小
    if (len < sizeof(dht11_data_buffer))
    {
        return -EINVAL; // 无效参数
    }
    if (copy_to_user(buf, dht11_data_buffer, sizeof(dht11_data_buffer)))
    {
        return -EFAULT; // 地址错误
    }
    return sizeof(dht11_data_buffer);
}

static struct file_operations dht11_ops = {
    .owner = THIS_MODULE,
    .open = dht11_open,
    .release = dht11_close,
    .read = dht11_read,
};

static int dht11_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct dht11_struct *my_data;

    pr_info("Probe: Device '%s' found!\n", pdev->name);

    // 1. 分配私有数据结构
    my_data = kmalloc(sizeof(struct dht11_struct), GFP_KERNEL);
    if (!my_data)
    {
        pr_err("Failed to allocate memory for device data.\n");
        return -ENOMEM;
    }
    // 将私有数据结构附加到设备实例上
    platform_set_drvdata(pdev, my_data);

    // 初始化私有数据结构，避免使用未初始化的值
    memset(my_data, 0, sizeof(struct dht11_struct));

    // 2. 分配设备号
    ret = alloc_chrdev_region(&(my_data->dev_number), 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("Failed to allocate major number.\n");
        goto err_free_data;
    }

    // 3. 初始化并添加字符设备
    cdev_init(&(my_data->cdev), &dht11_ops);
    my_data->cdev.owner = THIS_MODULE;
    ret = cdev_add(&(my_data->cdev), my_data->dev_number, 1);
    if (ret < 0)
    {
        pr_err("Failed to add character device.\n");
        goto err_unregister_region;
    }

    // 4. 创建设备类
    my_data->class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_data->class))
    {
        pr_err("Failed to create device class.\n");
        ret = PTR_ERR(my_data->class);
        goto err_cdev_del;
    }

    // 5. 获取GPIO资源
    // 推荐使用 devm_gpiod_get，它会自动处理 remove 时的释放
    my_data->pin = devm_gpiod_get(&pdev->dev, NULL, GPIOD_OUT_HIGH);
    if (IS_ERR(my_data->pin))
    {
        pr_err("Failed to get GPIO pin for DHT11.\n");
        ret = PTR_ERR(my_data->pin);
        goto err_class_destroy; // 如果获取失败，跳转到这里清理
    }

    // 6. 创建设备节点
    my_data->device = device_create(my_data->class, NULL, my_data->dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(my_data->device))
    {
        pr_err("Failed to create device node.\n");
        ret = PTR_ERR(my_data->device);
        goto err_dev_destroy; // 新增一个错误处理路径
    }

    pr_info("My device driver loaded successfully.\n");
    return 0;

err_dev_destroy:
    // 如果 device_create 失败，我们只需要销毁之前创建的类，
    // devm_gpiod_get 会在 device 销毁时自动释放 GPIO
    class_destroy(my_data->class);
    goto err_cdev_del; // 然后继续清理

err_class_destroy:
    class_destroy(my_data->class);
err_cdev_del:
    cdev_del(&(my_data->cdev));
err_unregister_region:
    unregister_chrdev_region(my_data->dev_number, 1);
err_free_data:
    kfree(my_data);
    platform_set_drvdata(pdev, NULL);
    return ret;
}

static int dht11_remove(struct platform_device *pdev)
{
    // 1. 从设备实例中获取之前附加的私有数据
    struct dht11_struct *my_data = platform_get_drvdata(pdev);

    // 检查指针是否有效
    if (!my_data)
    {
        pr_err("Remove: Private data pointer is NULL.\n");
        return -EINVAL;
    }

    pr_info("Remove: Unregistering device '%s'.\n", pdev->name);

    // 2. 销毁设备节点
    device_destroy(my_data->class, my_data->dev_number);

    // 3. 销毁设备类
    class_destroy(my_data->class);

    // 4. 删除字符设备
    cdev_del(&my_data->cdev);

    // 5. 注销设备号
    unregister_chrdev_region(my_data->dev_number, 1);

    // 6. 释放之前分配的内存
    kfree(my_data);

    // 7. 清除设备数据指针，避免悬空指针
    platform_set_drvdata(pdev, NULL);

    // 这里不需要 gpiod_put，因为 probe 中使用了 devm_gpiod_get

    pr_info("My device driver removed successfully.\n");

    return 0;
}

static const struct of_device_id match_table[] = {
    {
        .compatible = COMPATIBLE_NAME,
    },
    {/* Space */},
};
MODULE_DEVICE_TABLE(of, match_table);

static struct platform_driver dht11_pdrv = {
    .probe = dht11_probe,
    .remove = dht11_remove,
    .driver =
        {
            .name = KBUILD_MODNAME,
            .owner = THIS_MODULE,
            .of_match_table = match_table,
        },
};

module_platform_driver(dht11_pdrv);
MODULE_AUTHOR("CCoisini");
MODULE_DESCRIPTION("This is a test driver");
MODULE_LICENSE("GPL");