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

#include "common.h"
#include "klogging.h"

#ifndef KBUILD_MODNAME
#define KBUILD_MODNAME              DEV_NAME
#endif

#define PCAN_USB_EP_CMDOUT		    1
#define PCAN_USB_EP_CMDIN		    (PCAN_USB_EP_CMDOUT | USB_DIR_IN)
#define PCAN_USB_EP_MSGOUT		    2
#define PCAN_USB_EP_MSGIN		    (PCAN_USB_EP_MSGOUT | USB_DIR_IN)

#define PCAN_USB_MAX_TX_URBS        10

struct net_device;

typedef struct usb_forwarder
{
    struct net_device *net_dev;
    struct usb_device *usb_dev;
    struct usb_interface *usb_intf;
} usb_forwarder_t;

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

static int pcan_usb_plugin(struct usb_interface *interface, const struct usb_device_id *id)
{
    int err = -ENOMEM;
    struct net_device *netdev = NULL;
    usb_forwarder_t *forwarder = NULL;
    struct usb_host_interface *intf = interface->cur_altsetting;
    int i;

    return err;
    if (NULL == (netdev = alloc_candev(sizeof(usb_forwarder_t), PCAN_USB_MAX_TX_URBS)))
    {
        pr_err_v("alloc_candev() failed\n");
        return err;
    }
    SET_NETDEV_DEV(netdev,  &interface->dev); /* Set netdev's parent device. */
    netdev->flags |= IFF_ECHO; /* Support local echo. */
    /* TODO: netdev_ops */
    /* TODO: register_candev() */

    forwarder = netdev_priv(netdev);
    memset(forwarder, 0, sizeof(*forwarder));
    forwarder->net_dev = netdev;
    forwarder->usb_dev = interface_to_usbdev(interface);
    forwarder->usb_intf = interface;

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
            err = -ENODEV;
            goto probe_failed;
        }
    }

    usb_set_intfdata(interface, forwarder);

    pr_notice_v("New USB device with minor = %d\n", interface->minor);

    return 0;

probe_failed:

    /*unregister_candev(netdev);*/
    free_candev(netdev);

    return err;
}

static void pcan_usb_plugout(struct usb_interface *interface)
{
    usb_forwarder_t *forwarder = usb_get_intfdata(interface);

    if (NULL != forwarder)
    {
        /*unregister_candev(forwarder->netdev);*/
        free_candev(forwarder->net_dev);
        usb_set_intfdata(interface, NULL);
        pr_notice("Disconnected %s-%d.\n", DEV_NAME, interface->minor);
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-03, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

