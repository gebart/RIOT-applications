/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include "6lo_params.h"

#include "shell.h"
#include "msg.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#include "net/fib.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/ipv6/nc.h"
#include "net/gnrc/ipv6/netif.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netif.h"
#include "net/ipv6/addr.h"
#include "net/netdev2.h"
#include "net/netopt.h"
#include "net/gnrc/rpl.h"

#include "uhcp.h"
#include "fmt.h"

kernel_pid_t gnrc_border_interface;
kernel_pid_t gnrc_wireless_interface;

static void set_interface_roles(void)
{
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t numof = gnrc_netif_get(ifs);
    int res;
    uint16_t dev_type;

    for (size_t i = 0; i < numof && i < GNRC_NETIF_NUMOF; i++) {
        kernel_pid_t dev = ifs[i];
        res = gnrc_netapi_get(dev, NETOPT_DEVICE_TYPE, 0, &dev_type, sizeof(dev_type));
        if (res <= 0) {
            dev_type = NETDEV2_TYPE_UNKNOWN;
        }
        if ((!gnrc_border_interface) && (dev_type == NETDEV2_TYPE_ETHERNET)) {
            ipv6_addr_t addr, defroute;
            gnrc_border_interface = dev;

            ipv6_addr_from_str(&addr, "fe80::2");
            gnrc_ipv6_netif_add_addr(dev, &addr, 64,
                                     GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST);

            ipv6_addr_from_str(&defroute, "::");
            ipv6_addr_from_str(&addr, "fe80::1");
            fib_add_entry(&gnrc_ipv6_fib_table, dev, defroute.u8, 16,
                    FIB_FLAG_NET_PREFIX, addr.u8, 16, 0,
                    (uint32_t)FIB_LIFETIME_NO_EXPIRE);
        }
        else if ((!gnrc_wireless_interface) && (dev_type == NETDEV2_TYPE_UNKNOWN)) {
            gnrc_wireless_interface = dev;
        }

        if (gnrc_border_interface && gnrc_wireless_interface) {
            break;
        }
    }

    printf("Using %u as border interface and %u as wireless interface.\n", gnrc_border_interface, gnrc_wireless_interface);
}

static ipv6_addr_t _current_prefix;

void uhcp_handle_prefix(uint8_t *buf, uint8_t prefix_len, uint16_t lifetime, uint8_t *src, uhcp_iface_t iface)
{
    eui64_t iid;
    ipv6_addr_t new_prefix;

    memcpy(&new_prefix, buf, sizeof(ipv6_addr_t));

    if (!gnrc_wireless_interface) {
        puts("uhcp_handle_prefix(): received prefix, but don't know any wireless interface");
        return;
    }

    if (iface != gnrc_border_interface) {
        puts("uhcp_handle_prefix(): received prefix from unexpected interface");
        return;
    }

    if (gnrc_netapi_get(gnrc_wireless_interface, NETOPT_IPV6_IID, 0, &iid,
                        sizeof(eui64_t)) >= 0) {
        ipv6_addr_set_aiid(&new_prefix, iid.uint8);
    }
    else {
        puts("uhcp_handle_prefix(): cannot get IID of wireless interface");
        return;
    }

    if (ipv6_addr_equal(&_current_prefix, &new_prefix)) {
        puts("uhcp_handle_prefix(): got same prefix again");
        return;
    }

    gnrc_ipv6_netif_add_addr(gnrc_wireless_interface, &new_prefix, 64,
                             GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST |
                             GNRC_IPV6_NETIF_ADDR_FLAGS_NDP_AUTO);

    gnrc_ipv6_netif_remove_addr(gnrc_wireless_interface, &_current_prefix);
    print_str("uhcp_handle_prefix(): configured new prefix ");
    ipv6_addr_print(&new_prefix);
    puts("/64");

    puts("Starting RPL...");
    gnrc_rpl_init(gnrc_wireless_interface);
    gnrc_rpl_instance_t *inst;
    /* Use fixed instance number 0 */
    inst = gnrc_rpl_root_init(0, &new_prefix, false, false);
    if (inst == NULL) {
        print_str("uhcp_handle_prefix(): error initializing DODAG root ");
        ipv6_addr_print(&new_prefix);
        puts("");
    }

    ipv6_addr_set_iid(&new_prefix, 2ull);
    gnrc_ipv6_netif_add_addr(gnrc_border_interface, &new_prefix, 128,
                             GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST |
                             GNRC_IPV6_NETIF_ADDR_FLAGS_NDP_AUTO);

    if (!ipv6_addr_is_unspecified(&_current_prefix)) {
        gnrc_ipv6_netif_remove_addr(gnrc_wireless_interface, &_current_prefix);
        print_str("uhcp_handle_prefix(): removed old prefix ");
        ipv6_addr_print(&_current_prefix);
        puts("/64");
        ipv6_addr_set_iid(&_current_prefix, 2ull);
        gnrc_ipv6_netif_remove_addr(gnrc_border_interface, &_current_prefix);
    }

    ipv6_addr_set_aiid(&new_prefix, iid.uint8);
    memcpy(&_current_prefix, &new_prefix, sizeof(ipv6_addr_t));
}

extern void uhcp_client(uhcp_iface_t iface);

static char _uhcp_client_stack[THREAD_STACKSIZE_DEFAULT + THREAD_EXTRA_STACKSIZE_PRINTF];
static msg_t _uhcp_msg_queue[4];

void* uhcp_client_thread(void *arg)
{
    msg_init_queue(_uhcp_msg_queue, sizeof(_uhcp_msg_queue)/sizeof(msg_t));
    uhcp_client(gnrc_border_interface);
    return NULL;
}

void init_net(void)
{
    uint16_t val;
    int res;
    kernel_pid_t dev = gnrc_wireless_interface;

    if (dev == KERNEL_PID_UNDEF) {
        puts("No lowpan device found!");
    }

    val = lbr_config.channel;
    res = gnrc_netapi_set(dev, NETOPT_CHANNEL, 0, &val, sizeof(uint16_t));
    if (res < 0) {
        printf("Unable to set channel %"PRIu16", res=%d\n", val, res);
    }

    val = lbr_config.page;
    res = gnrc_netapi_set(dev, NETOPT_CHANNEL_PAGE, 0, &val, sizeof(uint16_t));
    if (res < 0) {
        printf("Unable to set page %"PRIu16", res=%d\n", val, res);
    }

    val = lbr_config.panid;
    res = gnrc_netapi_set(dev, NETOPT_NID, 0, &val, sizeof(uint16_t));
    if (res < 0) {
        printf("Unable to set PAN ID 0x%"PRIx16", res=%d\n", val, res);
    }
}

int main(void)
{
    set_interface_roles();
    init_net();

    /* initiate uhcp client */
    thread_create(_uhcp_client_stack, sizeof(_uhcp_client_stack),
                            THREAD_PRIORITY_MAIN - 1, THREAD_CREATE_STACKTEST,
                            uhcp_client_thread, NULL, "uhcp");

    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    puts("RIOT border router example application");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
