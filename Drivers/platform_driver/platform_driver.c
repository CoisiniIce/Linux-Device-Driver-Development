#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

static int my_pdrv_probe(struct platform_device *pdev)
{
    pr_info("hello\r\n");
    return 0;
}

static int my_pdrv_remove(struct platform_device *pdev)
{
    pr_info("good bye\r\n");
    return 0;
}


static struct platform_driver mydrv = {
    .probe = my_pdrv_probe,
    .remove = my_pdrv_remove,
    .driver =
        {
            .name = KBUILD_MODNAME,
            .owner = THIS_MODULE,
        },
};

static int __init mydevice_init(void)
{
    pr_info("Init\r\n");
    platform_driver_register(&mydrv);
    return 0;
}

static void __exit mydevice_exit(void)
{
    pr_info("Exit\r\n");
    platform_driver_unregister(&mydrv);
}

module_init(mydevice_init);
module_exit(mydevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux device driver");