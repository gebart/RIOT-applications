# name of your application
APPLICATION = 6lbr

# If no BOARD is found in the environment, use this default:
BOARD ?= mulle

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

# Name of virtual ethernet TAP interface
TAP_NAME ?= riot0

ROUTABLE_PREFIX ?= fdfd::

BOARD_INSUFFICIENT_MEMORY := airfy-beacon msb-430 msb-430h pca10000 pca10005 \
                             nrf51dongle nrf6310 nucleo-f103 nucleo-f334 \
                             spark-core stm32f0discovery telosb \
                             weio wsn430-v1_3b wsn430-v1_4 yunjia-nrf51822 z1

# Choose one: ethos or slip
LBR_PROTO ?= ethos

ifeq (slip,$(LBR_PROTO))
  # SLIP running on a free UART port
  ifeq (,$(SLIP_UART))
    # set default (last available UART)
    SLIP_UART=UART_DEV(UART_NUMOF-1)
  endif
  ifeq (,$(SLIP_BAUDRATE))
    # set default
    SLIP_BAUDRATE=115200
  endif
  CFLAGS += '-DSLIP_UART=$(SLIP_UART)'
  CFLAGS += '-DSLIP_BAUDRATE=$(SLIP_BAUDRATE)'
  # Include slip module for IP over Serial communication
  USEMODULE += gnrc_slip
else ifeq (ethos,$(LBR_PROTO))
  # ethos multiplexes console I/O and Ethernet frames over a single UART port
  ifneq (,$(ETHOS_UART))
    CFLAGS += '-DETHOS_UART=$(ETHOS_UART)'
  endif
  ifneq (,$(ETHOS_BAUDRATE))
    CFLAGS += '-DETHOS_BAUDRATE=$(ETHOS_BAUDRATE)'
  endif
  CFLAGS += -DUSE_ETHOS_FOR_STDIO
  # Include ethos module for ethernet over serial port implementation
  USEMODULE += ethos gnrc_netdev2
else
  $(error Choose either ethos or slip by setting LBR_PROTO=ethos or LBR_PROTO=slip)
endif

# We have two network interfaces (one 6lowpan, one UART-based)
GNRC_NETIF_NUMOF := 2
INCLUDES += -I$(CURDIR)

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += gnrc_netif_default
USEMODULE += auto_init_gnrc_netif

# Specify the mandatory networking modules for 6LoWPAN border router
USEMODULE += gnrc_sixlowpan_border_router_default
# Add forwarding table
USEMODULE += fib
# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

# Add RPL support (a mesh routing protocol)
USEMODULE += gnrc_rpl

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP

# Pass our chosen prefix to the build
CFLAGS += '-DROUTABLE_PREFIX="$(ROUTABLE_PREFIX)"'

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include

ifeq (,$(PORT))
  $(warning PORT unspecified)
endif

ETHOS_PROGRAM := $(RIOTBASE)/dist/tools/ethos/ethos

.PHONY: $(ETHOS_PROGRAM)

$(ETHOS_PROGRAM):
	$(MAKE) -C $(dir $(ETHOS_PROGRAM)) $(notdir $(ETHOS_PROGRAM))

connect-router: $(RIOTBASE)/dist/tools/ethos/ethos
	sudo sh -c 'ETHOS=${ETHOS_PROGRAM} $(CURDIR)/connect-router.sh $(PORT) $(TAP_NAME) $(ROUTABLE_PREFIX)'
