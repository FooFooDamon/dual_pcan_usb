/*
 * Sysfs mechanism.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "chardev_sysfs.h"

#include <linux/device.h>
#include <linux/sysfs.h>

#include "versions.h"
#include "common.h"
#include "chardev_ioctl.h"
#include "usb_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

static ssize_t version_show(struct class *cls, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", DRIVER_VERSION "-" __VER__);
}

static const struct class_attribute S_CLASS_ATTRS[] = {
    __ATTR_RO(version),
    {} /* trailing empty sentinel*/
};

const struct class_attribute* pcan_class_attributes(void)
{
    return S_CLASS_ATTRS;
}

static ssize_t hwtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", PRODUCT_TYPE);
}

static DEVICE_ATTR_RO(hwtype);

static ssize_t minor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", MINOR(((usb_forwarder_t *)dev_get_drvdata(dev))->char_dev.device->devt));
}

static DEVICE_ATTR_RO(minor);

static ssize_t dev_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "/dev/pcan%u\n", MINOR(((usb_forwarder_t *)dev_get_drvdata(dev))->char_dev.device->devt));
}

static DEVICE_ATTR_RO(dev_name);

static ssize_t nom_bitrate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", ((usb_forwarder_t *)dev_get_drvdata(dev))->can.bittiming.bitrate);
}

static DEVICE_ATTR_RO(nom_bitrate);

#define PCANFD_INIT_USER            0x80000000

static ssize_t init_flags_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%08x\n", PCANFD_INIT_USER);
}

static DEVICE_ATTR_RO(init_flags);

static ssize_t clock_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", ((usb_forwarder_t *)dev_get_drvdata(dev))->can.clock.freq);
}

static DEVICE_ATTR_RO(clock);

static ssize_t bus_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", PCANFD_ERROR_ACTIVE); /* TODO: More possibilities in future. */
}

static DEVICE_ATTR_RO(bus_state);

static ssize_t type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", DEV_TYPE);
}

static DEVICE_ATTR_RO(type);

static ssize_t read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    /* TODO: Include status messages. */
    return sprintf(buf, "%llu\n", ((usb_forwarder_t *)dev_get_drvdata(dev))->char_dev.rx_packets);
}

static DEVICE_ATTR_RO(read);

static ssize_t write_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", 0); /* FIXME: Implement it in future. */
}

static DEVICE_ATTR_RO(write);

static ssize_t rx_frames_counter_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%llu\n", ((usb_forwarder_t *)dev_get_drvdata(dev))->char_dev.rx_packets);
}

static DEVICE_ATTR_RO(rx_frames_counter);

static ssize_t tx_frames_counter_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", 0); /* FIXME: Implement it in future. */
}

static DEVICE_ATTR_RO(tx_frames_counter);

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "0x%04x\n", 0/* FIXME: Fetched from somewhere. */);
}

static DEVICE_ATTR_RO(status);

static ssize_t adapter_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", "dual_pcan_usb");
}

static DEVICE_ATTR_RO(adapter_name);

static ssize_t adapter_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", DRIVER_VERSION "-" __VER__);
}

static DEVICE_ATTR_RO(adapter_version);

static const struct attribute *S_DEV_ATTRS[] = {
    &dev_attr_hwtype.attr,
    &dev_attr_minor.attr,
    &dev_attr_dev_name.attr,
    &dev_attr_nom_bitrate.attr,
    &dev_attr_init_flags.attr,
    &dev_attr_clock.attr,
    &dev_attr_bus_state.attr,
    &dev_attr_type.attr,
    &dev_attr_read.attr,
    &dev_attr_write.attr,
    &dev_attr_rx_frames_counter.attr,
    &dev_attr_tx_frames_counter.attr,
    &dev_attr_status.attr,
    &dev_attr_adapter_name.attr,
    &dev_attr_adapter_version.attr,
    NULL /* trailing null sentinel*/
};

const struct attribute** pcan_device_attributes(void)
{
    return S_DEV_ATTRS;
}

#ifdef __cplusplus
}
#endif

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-12-18, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

