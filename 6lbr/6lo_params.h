/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef ROUTABLE_PREFIX
#define ROUTABLE_PREFIX "fdfd::"
#endif

static struct {
    uint16_t channel;
    uint16_t page;
    uint16_t panid;
    const char *prefix;
    const char *host_ip_suffix;
    const char *device_ip_suffix;
} lbr_config = {
    .channel = 0,
    .page = 2,
    .panid = 0x777,
    .prefix = ROUTABLE_PREFIX,
    .host_ip_suffix = "::1",
    .device_ip_suffix = "::2",
};

#define LBR_PREFIX_LENGTH 64
