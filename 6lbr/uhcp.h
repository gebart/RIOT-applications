#ifndef UHCP_H
#define UHCP_H

#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>

#define UHCP_MAGIC  (0x55484350) /* "UHCP" in hex */
#define UHCP_VER    (0)

typedef enum {
    UHCP_REQ,
    UHCP_PUSH
} uhcp_type_t;

typedef struct __attribute__((packed)) {
    uint32_t uhcp_magic;
    uint8_t ver_type;
} uhcp_hdr_t;

typedef struct __attribute__((packed)) {
    uhcp_hdr_t hdr;
    uint8_t prefix_len;
} uhcp_req_t;

typedef struct __attribute__((packed)) {
    uhcp_hdr_t hdr;
    uint8_t prefix_len;
    uint8_t prefix[];
} uhcp_push_t;

typedef unsigned uhcp_iface_t;

void uhcp_handle_udp(uint8_t *buf, size_t len, uint8_t *src, uint16_t port, uhcp_iface_t iface);
void uhcp_handle_req(uhcp_req_t *req, uint8_t *src, uint16_t port, uhcp_iface_t iface);
void uhcp_handle_push(uhcp_push_t *req, uint8_t *src, uint16_t port, uhcp_iface_t iface);

void uhcp_handle_prefix(uint8_t *prefix, uint8_t prefix_len, uint16_t lifetime, uint8_t *src, uhcp_iface_t iface);

static inline void uhcp_hdr_set(uhcp_hdr_t *hdr, uhcp_type_t type)
{
    hdr->uhcp_magic = htonl(UHCP_MAGIC);
    hdr->ver_type = (UHCP_VER << 4) | (type & 0xF);
}

int udp_sendto(uint8_t *buf, size_t len, uint8_t *dst, uint16_t dst_port, uhcp_iface_t dst_iface);

#endif /* UHCP_H */
