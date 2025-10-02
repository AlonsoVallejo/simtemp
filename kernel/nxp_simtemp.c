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

#define NEW_SAMPLE 0x1
#define THRESHOLD_CROSSED 0x2

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

/* Simulated temperature state */

static int simtemp_current_mC = 44000; // 44.0C initial
static struct timer_list simtemp_timer;
static DEFINE_MUTEX(simtemp_lock);

static wait_queue_head_t simtemp_wq;
static unsigned int simtemp_sample_seq = 0; // increments on each new sample

/* Functions prototypes */
static void simtemp_update_temp(struct timer_list *t);
static void simtemp_mode_normal(void);
static void simtemp_mode_noisy(void);
static void simtemp_mode_ramp(void);

/**
 * @brief Timer callback to update the simulated temperature value.
 * @param[in] t Pointer to the timer_list structure (unused).
 */
/**
 * @brief Timer callback to update the simulated temperature value.
 * @param[in] t Pointer to the timer_list structure (unused).
 */
static void simtemp_update_temp(struct timer_list *t)
{
    mutex_lock(&simtemp_lock);

    if (strcmp(mode, "noisy") == 0) {
        simtemp_mode_noisy();
    } else if (strcmp(mode, "ramp") == 0) {
        simtemp_mode_ramp();
    } else if (strcmp(mode, "normal") == 0) {
        simtemp_mode_normal();
    } else {
        /* invalid entered mode, use last valid mode */
    }

    stats_updates++;
    simtemp_sample_seq++;
    mutex_unlock(&simtemp_lock);

    wake_up_interruptible(&simtemp_wq);
    mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(sampling_ms));
}

/**
 * @brief Normal mode simulation. Increases temperature by 0.1C every tick, wraps at 46.0C.
 * @return void
*/
static void simtemp_mode_normal(void)
{
    simtemp_current_mC += 10; /* 0.1C per tick */
    if (simtemp_current_mC > 46000) {
        /* Wrap around */
        simtemp_current_mC = 44000;
    }
}

/**
 * @brief Noisy mode simulation. Adds random noise to the temperature.
 * @return void
 */
static void simtemp_mode_noisy(void)
{
    int noise = (get_random_u32() % 200) - 100; /* Random noise between -100 and +100 mC */
    simtemp_current_mC += noise;
    if (simtemp_current_mC > 46000) {
        simtemp_current_mC = 46000;
    }
    if (simtemp_current_mC < 44000) {
        simtemp_current_mC = 44000;
    }
}

/**
 * @brief Ramp mode simulation. Ramps temperature up and down between 44.0C and 46.0C.
 * @return void
 */
static void simtemp_mode_ramp(void)
{
    static int direction = 1;
    simtemp_current_mC += direction * 50;  /* 0.05C per tick */
    if (simtemp_current_mC >= 46000) { 
        direction = -1;
    }

    if (simtemp_current_mC <= 44000) {
        direction = 1;
    }
}

/* Sysfs show/store functions */
/**
 * @brief Show function for sampling_ms sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[out] buf Output buffer for value.
 * @return Number of bytes written to buf.
 */
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", sampling_ms);
}

/**
 * @brief Store function for sampling_ms sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[in] buf Input buffer with value.
 * @param[in] count Number of bytes in buf.
 * @return Number of bytes consumed, or negative error code.
 */
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int val;
    /* check for valid input values */
    if ( ( kstrtouint(buf, 10, &val) < 0 ) || (val == 0) ) {
        stats_last_error = -EINVAL;
        return -EINVAL;
    }

    sampling_ms = val;
    
    return count;
}
static DEVICE_ATTR_RW(sampling_ms); /* Read/Write attribute */

/**
 * @brief Show function for threshold_mC sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[out] buf Output buffer for value.
 * @return Number of bytes written to buf.
 */
static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", threshold_mC);
}

/**
 * @brief Store function for threshold_mC sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[in] buf Input buffer with value.
 * @param[in] count Number of bytes in buf.
 * @return Number of bytes consumed, or negative error code.
 */
static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val) < 0) {
        /* Invalid input */
        stats_last_error = -EINVAL;
        return -EINVAL;
    }

    threshold_mC = val;
    
    return count;
}
static DEVICE_ATTR_RW(threshold_mC); /* Read/Write attribute */

/**
 * @brief Show function for mode sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[out] buf Output buffer for value.
 * @return Number of bytes written to buf.
 */
static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", mode);
}

/**
 * @brief Store function for mode sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[in] buf Input buffer with value.
 * @param[in] count Number of bytes in buf.
 * @return Number of bytes consumed, or negative error code.
 */
static ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    size_t len = min(count, sizeof(mode)-1);
    char tmp[16];

    memcpy(tmp, buf, len);
    tmp[len] = '\0';

    /* Remove trailing newline if present */
    if (len > 0 && tmp[len-1] == '\n')
        tmp[len-1] = '\0';

    /* Validate mode */
    if (strcmp(tmp, "normal") != 0 &&
        strcmp(tmp, "noisy") != 0 &&
        strcmp(tmp, "ramp") != 0) {
        stats_last_error = -EINVAL;
        return -EINVAL;
    }

    strcpy(mode, tmp);
    return count;
}
static DEVICE_ATTR_RW(mode); /* Read/Write attribute */

/**
 * @brief Show function for stats sysfs attribute.
 * @param[in] dev Device pointer.
 * @param[in] attr Device attribute pointer.
 * @param[out] buf Output buffer for value.
 * @return Number of bytes written to buf.
 */
static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "updates=%u\nalerts=%u\nlast_error=%d\n", stats_updates, stats_alerts, stats_last_error);
}
static DEVICE_ATTR_RO(stats); /* Read-Only attribute */

/**
 * @brief Open function for the simtemp character device.
 * @param[in] inode Inode pointer.
 * @param[in] file File pointer.
 * @return 0 on success.
 */
static int simtemp_open(struct inode *inode, struct file *file)
{
    pr_info("nxp_simtemp: device opened\n");
    return 0;
}

/**
 * @brief Release function for the simtemp character device.
 * @param[in] inode Inode pointer.
 * @param[in] file File pointer.
 * @return 0 on success.
 */
static int simtemp_release(struct inode *inode, struct file *file)
{
    pr_info("nxp_simtemp: device closed\n");
    return 0;
}

/* Data structure for a temperature sample */
struct simtemp_sample {
    __u64 timestamp_ns;
    __s32 temp_mC;
    __u32 flags;
} __attribute__((packed));

/**
 * @brief Read function for the simtemp character device.
 * @param[in] file File pointer.
 * @param[out] buf User buffer to copy data to.
 * @param[in] count Number of bytes to read.
 * @param[in,out] ppos File position pointer.
 * @return Number of bytes read, or negative error code.
 */
/**
 * @brief Read function for the simtemp character device.
 * @param[in] file File pointer.
 * @param[out] buf User buffer to copy data to.
 * @param[in] count Number of bytes to read.
 * @param[in,out] ppos File position pointer.
 * @return Number of bytes read, or negative error code.
 */
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct simtemp_sample sample;
    static DEFINE_MUTEX(local_lock); /* for per-file state */
    static int last_alert_state = 0; /* 0: below, 1: above or equal */

    /* Wait for a new sample (block until simtemp_sample_seq changes) */
    mutex_lock(&local_lock);
    unsigned int prev_seq;
    mutex_lock(&simtemp_lock);
    prev_seq = simtemp_sample_seq;
    mutex_unlock(&simtemp_lock);

    if (wait_event_interruptible(simtemp_wq, ({
        unsigned int cur_seq;
        mutex_lock(&simtemp_lock);
        cur_seq = simtemp_sample_seq;
        mutex_unlock(&simtemp_lock);
        cur_seq != prev_seq;
    })) < 0)
    {
        mutex_unlock(&local_lock);
        /* Interrupted by signal */
        stats_last_error = -ERESTARTSYS;
        return -ERESTARTSYS;
    }

    mutex_lock(&simtemp_lock);
    sample.timestamp_ns = ktime_get_ns();
    sample.temp_mC = simtemp_current_mC;
    sample.flags = NEW_SAMPLE;
    int alert = (simtemp_current_mC >= threshold_mC) ? 1 : 0;
    if (alert) {
        sample.flags |= THRESHOLD_CROSSED;
    }
    /* Detect threshold crossing (edge) */
    if (alert != last_alert_state) {
        stats_alerts++;
        last_alert_state = alert;
    }
    mutex_unlock(&simtemp_lock);
    mutex_unlock(&local_lock);

    if (count < sizeof(sample)) {
        /* Buffer too small */
        stats_last_error = -EINVAL;
        return -EINVAL;
    }

    if (copy_to_user(buf, &sample, sizeof(sample))) {
        /* Failed to copy to user */
        stats_last_error = -EFAULT;
        return -EFAULT;
    }

    return sizeof(sample);
}

static struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .open = simtemp_open,
    .release = simtemp_release,
    .read = simtemp_read,
};


/**
 * @brief Probe function for the nxp_simtemp platform driver.
 * @param[in] pdev Platform device pointer.
 * @return 0 on success, negative error code on failure.
 */
/**
 * @brief Probe function for the nxp_simtemp platform driver.
 * @param[in] pdev Platform device pointer.
 * @return 0 on success, negative error code on failure.
 */
static int nxp_simtemp_probe(struct platform_device *pdev)
{
    int ret;
    pr_info("nxp_simtemp: probe\n");

    /* Initialize wait queue and sample sequence */
    init_waitqueue_head(&simtemp_wq);
    simtemp_sample_seq = 0;

    /* Initialize temperature simulation timer */
    timer_setup(&simtemp_timer, simtemp_update_temp, 0);
    mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(sampling_ms));

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


/**
 * @brief Remove function for the nxp_simtemp platform driver.
 * @param[in] pdev Platform device pointer.
 */
static void nxp_simtemp_remove(struct platform_device *pdev)
{
    pr_info("nxp_simtemp: remove\n");
    del_timer_sync(&simtemp_timer);
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

/**
 * @brief Module initialization function.
 * @return 0 on success, negative error code on failure.
 */
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

/**
 * @brief Module exit function.
 */
static void __exit nxp_simtemp_exit(void)
{
    pr_info("nxp_simtemp: exit\n");

    del_timer_sync(&simtemp_timer);
    
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
