
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include "nxp_simtemp.h"

#define SIMTEMP_DEVICE_NAME "simtemp"
#define SIMTEMP_CLASS_NAME  "simtemp_class"

static dev_t simtemp_dev_num;
static struct class *simtemp_class = NULL;
static struct cdev simtemp_cdev;
static struct device *simtemp_device = NULL;
static struct platform_device *test_pdev = NULL;

/* Sysfs attribute variables */
static unsigned int sampling_ms = 100;
static int threshold_mC = 45000;
static char mode[16] = "normal";
static unsigned int stats_updates = 0;
static unsigned int stats_alerts = 0;
static int stats_last_error = 0;

/* Sysfs show/store functions */
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", sampling_ms);
}
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int val;
    if (kstrtouint(buf, 10, &val) < 0 || val == 0)
        return -EINVAL;
    sampling_ms = val;
    return count;
}
static DEVICE_ATTR_RW(sampling_ms);

static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", threshold_mC);
}
static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val) < 0)
        return -EINVAL;
    threshold_mC = val;
    return count;
}
static DEVICE_ATTR_RW(threshold_mC);

static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", mode);
}
static ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    size_t len = min(count, sizeof(mode)-1);
    memcpy(mode, buf, len);
    mode[len] = '\0';
    /* Remove trailing newline if present */
    if (len > 0 && mode[len-1] == '\n')
        mode[len-1] = '\0';
    return count;
}
static DEVICE_ATTR_RW(mode);

static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "updates=%u\nalerts=%u\nlast_error=%d\n", stats_updates, stats_alerts, stats_last_error);
}
static DEVICE_ATTR_RO(stats);

static int simtemp_open(struct inode *inode, struct file *file)
{
    pr_info("nxp_simtemp: device opened\n");
    return 0;
}

static int simtemp_release(struct inode *inode, struct file *file)
{
    pr_info("nxp_simtemp: device closed\n");
    return 0;
}

static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    pr_info("nxp_simtemp: read called\n");
    return 0; /* No data yet */
}

static struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .open = simtemp_open,
    .release = simtemp_release,
    .read = simtemp_read,
};


static int nxp_simtemp_probe(struct platform_device *pdev)
{
    int ret;
    pr_info("nxp_simtemp: probe\n");

    /* Allocate device number */
    ret = alloc_chrdev_region(&simtemp_dev_num, 0, 1, SIMTEMP_DEVICE_NAME);
    if (ret < 0) {
        pr_err("nxp_simtemp: failed to alloc chrdev region\n");
        return ret;
    }

    /* Create device class */
    simtemp_class = class_create(SIMTEMP_CLASS_NAME);
    if (IS_ERR(simtemp_class)) {
        pr_err("nxp_simtemp: failed to create class\n");
        unregister_chrdev_region(simtemp_dev_num, 1);
        return PTR_ERR(simtemp_class);
    }

    /* Create device */
    simtemp_device = device_create(simtemp_class, NULL, simtemp_dev_num, NULL, SIMTEMP_DEVICE_NAME);
    if (IS_ERR(simtemp_device)) {
        pr_err("nxp_simtemp: failed to create device\n");
        class_destroy(simtemp_class);
        unregister_chrdev_region(simtemp_dev_num, 1);
        return PTR_ERR(simtemp_device);
    }

    /* Init and add cdev */
    cdev_init(&simtemp_cdev, &simtemp_fops);
    ret = cdev_add(&simtemp_cdev, simtemp_dev_num, 1);
    if (ret < 0) {
        pr_err("nxp_simtemp: failed to add cdev\n");
        device_destroy(simtemp_class, simtemp_dev_num);
        class_destroy(simtemp_class);
        unregister_chrdev_region(simtemp_dev_num, 1);
        return ret;
    }

    /* Create sysfs attributes */
    ret = device_create_file(simtemp_device, &dev_attr_sampling_ms);
    if (ret)
        pr_warn("nxp_simtemp: failed to create sysfs sampling_ms\n");
    ret = device_create_file(simtemp_device, &dev_attr_threshold_mC);
    if (ret)
        pr_warn("nxp_simtemp: failed to create sysfs threshold_mC\n");
    ret = device_create_file(simtemp_device, &dev_attr_mode);
    if (ret)
        pr_warn("nxp_simtemp: failed to create sysfs mode\n");
    ret = device_create_file(simtemp_device, &dev_attr_stats);
    if (ret)
        pr_warn("nxp_simtemp: failed to create sysfs stats\n");

    pr_info("nxp_simtemp: char device and sysfs attributes created successfully\n");
    return 0;
}


static void nxp_simtemp_remove(struct platform_device *pdev)
{
    pr_info("nxp_simtemp: remove\n");
    if (simtemp_device) {
        device_remove_file(simtemp_device, &dev_attr_sampling_ms);
        device_remove_file(simtemp_device, &dev_attr_threshold_mC);
        device_remove_file(simtemp_device, &dev_attr_mode);
        device_remove_file(simtemp_device, &dev_attr_stats);
    }
    cdev_del(&simtemp_cdev);
    if (simtemp_device)
        device_destroy(simtemp_class, simtemp_dev_num);
    if (simtemp_class)
        class_destroy(simtemp_class);
    unregister_chrdev_region(simtemp_dev_num, 1);
}

static const struct of_device_id nxp_simtemp_of_match[] = {
    { .compatible = "nxp,simtemp" },
    { }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_of_match);

static struct platform_driver nxp_simtemp_driver = {
    .driver = {
        .name = "nxp_simtemp",
        .of_match_table = nxp_simtemp_of_match,
    },
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
};

static int __init nxp_simtemp_init(void)
{
    int ret;
    pr_info("nxp_simtemp: init\n");
    
    /* Register platform driver first */
    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        pr_err("nxp_simtemp: failed to register platform driver\n");
        return ret;
    }
    
    /* Create a test platform device for systems without device tree */
    test_pdev = platform_device_alloc("nxp_simtemp", 0);
    if (!test_pdev) {
        pr_err("nxp_simtemp: failed to allocate platform device\n");
        platform_driver_unregister(&nxp_simtemp_driver);
        return -ENOMEM;
    }
    
    ret = platform_device_add(test_pdev);
    if (ret) {
        pr_err("nxp_simtemp: failed to add platform device\n");
        platform_device_put(test_pdev);
        platform_driver_unregister(&nxp_simtemp_driver);
        return ret;
    }
    
    return 0;
}

static void __exit nxp_simtemp_exit(void)
{
    pr_info("nxp_simtemp: exit\n");
    
    /* Remove test platform device */
    if (test_pdev) {
        platform_device_unregister(test_pdev);
    }
    
    platform_driver_unregister(&nxp_simtemp_driver);
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_AUTHOR("AlonsoVallejo <jvallejorios@hotmail.com>");
MODULE_DESCRIPTION("NXP Simulated Temperature Sensor");
MODULE_LICENSE("GPL");
