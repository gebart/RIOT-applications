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

#include "shell.h"
#include "msg.h"
#include "6lo_params.h"
#include "net/fib.h"
#include "net/gnrc/ipv6.h"
#include "net/ipv6/addr.h"
#include "net/netdev2.h"
#include "net/netopt.h"
#include "net/gnrc/rpl.h"
#include "net/gnrc/netapi.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

kernel_pid_t lbr_tunnel_pid = KERNEL_PID_UNDEF;
kernel_pid_t lbr_lowpan_pid = KERNEL_PID_UNDEF;

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
        if ((lbr_tunnel_pid == KERNEL_PID_UNDEF) && (dev_type == NETDEV2_TYPE_ETHERNET)) {
            ipv6_addr_t addr;
            ipv6_addr_t defroute;
            ipv6_addr_set_unspecified(&defroute);
            lbr_tunnel_pid = dev;

            ipv6_addr_from_str(&addr, lbr_config.device_ip_suffix);
            ipv6_addr_set_link_local_prefix(&addr);
            gnrc_ipv6_netif_add_addr(dev, &addr, 64,
                                     GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST);

            ipv6_addr_from_str(&addr, lbr_config.host_ip_suffix);
            ipv6_addr_set_link_local_prefix(&addr);
            fib_add_entry(&gnrc_ipv6_fib_table, dev, &defroute.u8[0], 16,
                    FIB_FLAG_NET_PREFIX, &addr.u8[0], 16, 0,
                    (uint32_t)FIB_LIFETIME_NO_EXPIRE);
        }
        else if ((lbr_lowpan_pid == KERNEL_PID_UNDEF) && (dev_type == NETDEV2_TYPE_UNKNOWN)) {
            eui64_t iid;
            ipv6_addr_t addr;
            ipv6_addr_from_str(&addr, lbr_config.prefix);
            lbr_lowpan_pid = dev;
            if (gnrc_netapi_get(lbr_lowpan_pid, NETOPT_IPV6_IID, 0, &iid,
                                sizeof(eui64_t)) >= 0) {
                ipv6_addr_set_aiid(&addr, iid.uint8);
            }
            else {
                printf("cannot get IID of wireless interface %u\n", lbr_lowpan_pid);
                lbr_lowpan_pid = KERNEL_PID_UNDEF;
                continue;
            }
            gnrc_ipv6_netif_add_addr(lbr_lowpan_pid, &addr, LBR_PREFIX_LENGTH,
                                     GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST |
                                     GNRC_IPV6_NETIF_ADDR_FLAGS_NDP_AUTO);
        }

        if (lbr_tunnel_pid && lbr_lowpan_pid) {
            break;
        }
    }

    printf("Using %u as border interface and %u as wireless interface.\n", lbr_tunnel_pid, lbr_lowpan_pid);
}

void init_net(void)
{
    uint16_t val;
    int res;
    kernel_pid_t dev = lbr_lowpan_pid;

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

    ipv6_addr_t dodag_id;

    if (ipv6_addr_from_str(&dodag_id, lbr_config.prefix) == NULL) {
        printf("error: lbr_config.prefix=\"%s\" must be a valid IPv6 address\n", lbr_config.prefix);
        return;
    }
    eui64_t iid;
    if (gnrc_netapi_get(lbr_lowpan_pid, NETOPT_IPV6_IID, 0, &iid,
                        sizeof(eui64_t)) >= 0) {
        ipv6_addr_set_aiid(&dodag_id, iid.uint8);
    }
    else {
        printf("cannot get IID of wireless interface %u\n", lbr_lowpan_pid);
        lbr_lowpan_pid = KERNEL_PID_UNDEF;
        return;
    }

    gnrc_rpl_init(dev);
    gnrc_rpl_instance_t *inst;
    inst = gnrc_rpl_root_init(0, &dodag_id, true, false);
    if (inst == NULL) {
        char addr_str[IPV6_ADDR_MAX_STR_LEN];
        printf("error: initializing DODAG root (%s)\n",
                ipv6_addr_to_str(addr_str, &dodag_id, sizeof(addr_str)));
        return;
    }
}

/* import "ifconfig" shell command, used for printing addresses */
extern int _netif_config(int argc, char **argv);

/* import "fib" shell command, used for printing FIB */
extern int _fib_route_handler(int argc, char **argv);

/* import "rpl show" shell command, used for printing RPL info */
extern int _gnrc_rpl_dodag_show(void);

static char *_noop_argv[] = {"", NULL};

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT border router example application");

    set_interface_roles();
    init_net();

    /* print network addresses */
    puts("Configured network interfaces:");
    _netif_config(0, NULL);
    puts("RPL info:");
    _gnrc_rpl_dodag_show();
    puts("FIB:");
    _fib_route_handler(1, _noop_argv);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
