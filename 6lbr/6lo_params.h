/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

static struct {
    uint16_t channel;
    uint16_t page;
    uint16_t panid;
} lbr_config = {
    .channel = 0,
    .page = 2,
    .panid = 0x777,
};
