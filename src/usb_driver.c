/*
 * Implementation of USB submodule of PCAN-USB..
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include "usb_driver.h"

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/can/dev.h>
#include <linux/timer.h>

#include "common.h"
#include "klogging.h"
#include "can_commands.h"

#ifndef KBUILD_MODNAME
#define KBUILD_MODNAME                  DEV_NAME
#endif

#define PCAN_USB_EP_CMDOUT		        1
#define PCAN_USB_EP_CMDIN		        (PCAN_USB_EP_CMDOUT | USB_DIR_IN)
#define PCAN_USB_EP_MSGOUT		        2
#define PCAN_USB_EP_MSGIN		        (PCAN_USB_EP_MSGOUT | USB_DIR_IN)

#define PCAN_USB_MSG_TIMEOUT_MS         1000
#define PCAN_USB_STARTUP_TIMEOUT_MS     10

#define PCAN_USB_MAX_TX_URBS            10

#define PCAN_USB_MAX_CMD_LEN            32

static struct usb_device_id s_usb_ids[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) }
    , {}
};
MODULE_DEVICE_TABLE(usb, s_usb_ids);

static int pcan_usb_plugin(struct usb_interface *interface, const struct usb_device_id *id);
static void pcan_usb_plugout(struct usb_interface *interface);

static struct usb_driver s_driver = {
    .name = DEV_NAME
    , .id_table = s_usb_ids
    , .probe = pcan_usb_plugin
    , .disconnect = pcan_usb_plugout
};

int usbdrv_register(void)
{
    int ret = usb_register(&s_driver);

    if (ret)
        pr_err_v("usb_register() failed: %d\n", ret);

    return ret;
}

void usbdrv_unregister(void)
{
    usb_deregister(&s_driver);
}

int usbdrv_bulk_msg_send(usb_forwarder_t *forwarder, void *data, int len)
{
    return usb_bulk_msg(
        forwarder->usb_dev, usb_sndbulkpipe(forwarder->usb_dev, PCAN_USB_EP_CMDOUT),
        data, len, /* actual_length = */NULL, PCAN_USB_MSG_TIMEOUT_MS
    );
}

int usbdrv_bulk_msg_recv(usb_forwarder_t *forwarder, void *data, int len)
{
    return usb_bulk_msg(
        forwarder->usb_dev, usb_rcvbulkpipe(forwarder->usb_dev, PCAN_USB_EP_CMDIN),
        data, len, /* actual_length = */NULL, PCAN_USB_MSG_TIMEOUT_MS
    );
}

static inline int check_endpoints(const struct usb_interface *interface)
{
    struct usb_host_interface *intf = interface->cur_altsetting;
    int i;

    for (i = 0; i < intf->desc.bNumEndpoints; ++i)
    {
        struct usb_endpoint_descriptor *endpoint = &intf->endpoint[i].desc;

        switch (endpoint->bEndpointAddress)
        {
        case PCAN_USB_EP_CMDOUT:
        case PCAN_USB_EP_CMDIN:
        case PCAN_USB_EP_MSGOUT:
        case PCAN_USB_EP_MSGIN:
            break;

        default:
            return -ENODEV;
        }
    }

    return 0;
}

#ifdef setup_timer
#pragma message("Using old style timer APIs.")
static void pcan_usb_restart_callback(unsigned long arg)
{
    /*usb_forwarder_t *forwarder = (usb_forwarder_t *)arg;*/
#else
#pragma message("Using new style timer APIs.")
static void pcan_usb_restart_callback(struct timer_list *timer)
{
    /*usb_forwarder_t *forwarder = container_of(timer, usb_forwarder_t, restart_timer);*/
#endif

    /*netif_wake_queue(forwarder->net_dev);*/
}

static int pcan_usb_reset_bus(usb_forwarder_t *forwarder, unsigned char is_on)
{
    int err = pcan_set_bus(forwarder, is_on);

    if (err)
        return err;

    if (is_on)
    {
        /* Need some time to finish initialization. */
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT_MS));
    }
    else
        err = pcan_init_sja1000(forwarder);

    return err;
}

static int check_device_info(usb_forwarder_t *forwarder)
{
    u32 serial_number = 0;
    u32 device_id = 0;
    int err = pcan_get_serial_number(forwarder, &serial_number);

    if (err < 0)
    {
        pr_err_v("Unable to read serial number, err = %d\n", err);
        return err;
    }
    else
        dev_notice(&forwarder->usb_dev->dev, "Got serial number: 0x%08X\n", serial_number);

    if ((err = pcan_get_device_id(forwarder, &device_id)) < 0)
        pr_err_v("Unable to read device id, err = %d\n", err);
    else
        dev_notice(&forwarder->usb_dev->dev, "Got device id: %u\n", device_id);

    return err;
}

static int pcan_usb_plugin(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct net_device *netdev = NULL;
    usb_forwarder_t *forwarder = NULL;
    int err = check_endpoints(interface);

    if (err)
        return err;

    if (NULL == (netdev = alloc_candev(sizeof(usb_forwarder_t), PCAN_USB_MAX_TX_URBS)))
    {
        pr_err_v("alloc_candev() failed\n");
        return -ENOMEM;
    }
    SET_NETDEV_DEV(netdev,  &interface->dev); /* Set netdev's parent device. */
    netdev->flags |= IFF_ECHO; /* Support local echo. */
    /* TODO: netdev_ops */

    forwarder = netdev_priv(netdev);
    memset(forwarder, 0, sizeof(*forwarder));
    if (NULL == (forwarder->cmd_buf = kmalloc(PCAN_USB_MAX_CMD_LEN, GFP_KERNEL)))
    {
        err = -ENOMEM;
        pr_err_v("kmalloc() for cmd_buf failed\n");
        goto probe_failed;
    }
    forwarder->net_dev = netdev;
    forwarder->usb_dev = interface_to_usbdev(interface);
    forwarder->usb_intf = interface;
    forwarder->state |= PCAN_USB_STATE_CONNECTED;

    /* TODO: register_candev() */

    if ((err = check_device_info(forwarder)) < 0)
        goto probe_failed;

#ifdef setup_timer
    setup_timer(&forwarder->restart_timer, pcan_usb_restart_callback, (unsigned long)forwarder);
#else
    timer_setup(&forwarder->restart_timer, pcan_usb_restart_callback, /* flags = */0);
#endif

    usb_set_intfdata(interface, forwarder);

    dev_notice(&forwarder->usb_dev->dev, "New PCAN-USB device plugged in\n");

    return 0;

probe_failed:

    /*unregister_candev(netdev);*/

    if (NULL != forwarder->cmd_buf)
        kfree(forwarder->cmd_buf);

    free_candev(netdev);

    return err;
}

static void pcan_usb_plugout(struct usb_interface *interface)
{
    usb_forwarder_t *forwarder = usb_get_intfdata(interface);

    if (NULL != forwarder)
    {
        forwarder->state &= ~PCAN_USB_STATE_CONNECTED; /* Clear it as soon as possible. */
        /*unregister_candev(forwarder->netdev);*/
        /* TODO: Wait and free forwarder depending on reference counting mechanism. */
        kfree(forwarder->cmd_buf);
        free_candev(forwarder->net_dev);
        usb_set_intfdata(interface, NULL);
        dev_notice(&forwarder->usb_dev->dev, "PCAN-USB device plugged out\n");
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-03, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Remove definition of struct usb_forwarder_t.
 *  02. Add usbdrv_bulk_msg_{send,recv}() and pcan_usb_reset_bus().
 *  03. Add device state management, CAN command buffer management
 *      and device info checking.
 */

