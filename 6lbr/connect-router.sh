#!/bin/sh

set -x

HOST_IP_SUFFIX=1
DEVICE_IP_SUFFIX=2
LINKLOCAL_PREFIX=fe80::

create_tap() {
    ip tuntap add ${TAP_NAME} mode tap user $(id -u)
    sysctl -w net.ipv6.conf.${TAP_NAME}.forwarding=1
    sysctl -w net.ipv6.conf.${TAP_NAME}.accept_ra=0
    ip link set ${TAP_NAME} up
    ip address add ${ETHOS_HOST_LL_IP}/64 dev ${TAP_NAME}
    ip address add ${ETHOS_HOST_ROUTABLE_IP}/128 dev ${TAP_NAME}
    ip route add ${ROUTABLE_PREFIX}/64 via ${ETHOS_DEVICE_LL_IP} dev ${TAP_NAME}
}

remove_tap() {
    ip tuntap del ${TAP_NAME} mode tap
}

cleanup() {
    echo "Cleaning up..."
    remove_tap
    kill $UHCPD_PID
}

start_uhcpd() {
    ${UHCPD} ${TAP_NAME} ${ROUTABLE_PREFIX}/64 > /dev/null &
    UHCPD_PID=$!
}

if [ $# != 3 ]; then
    echo "Usage: $0 <tty_dev> <tap_name> <ipv6_prefix>"
    echo
    echo "IPv6 prefix length is always /64 and must not be specified."
    exit 1
fi

trap "cleanup" EXIT #SIGINT SIGTERM

PORT_DEV=$1
TAP_NAME=$2
ROUTABLE_PREFIX=$3

ETHOS_HOST_LL_IP=${LINKLOCAL_PREFIX}${HOST_IP_SUFFIX}
ETHOS_DEVICE_LL_IP=${LINKLOCAL_PREFIX}${DEVICE_IP_SUFFIX}
ETHOS_HOST_ROUTABLE_IP=${ROUTABLE_PREFIX}${HOST_IP_SUFFIX}

create_tap
start_uhcpd
${ETHOS} ${TAP_NAME} ${PORT_DEV}
