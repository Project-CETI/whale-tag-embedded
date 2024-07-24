#-----------------------------------------------------------------------------
# CETI Tag Electronics
# Cummings Electronics Labs, October 2021
# Developed under contract for Harvard University Wood Lab
#-----------------------------------------------------------------------------
# Version		Date		Description
#	0.00    10/08/21   Begin work, establish framework
#       2.00    12/06/21   Port over to github.com/Project-CETI/whale-tag-embedded, add to debian build
#       2.1.0   03/15/22   Target the summer 2022 deployment based on hardware v2.1
#       2.2.0   09/15/22   Target raspberry pi zero 2 w, wich is arm64 cortex a53 1Ghz quad core
#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------
# Description
# This is the CETI makefile for the top level
#-----------------------------------------------------------------------------
SRC_DIR = src

HAL_SRC	   = $(shell find $(SRC_DIR)/hal -type f -iname '*.c' 2> /dev/null)
HAL_OBJ	   = $(HAL_SRC:.c=.o)

CAPTURE_APP = cetiTagApp
CAPTURE_SRC = $(shell find $(SRC_DIR)/$(CAPTURE_APP) -type f -iname '*.c' 2> /dev/null)
CAPTURE_OBJ = $(CAPTURE_SRC:.c=.o)

HW_TEST_APP = cetiHWTestApp
HW_TEST_SRC = $(shell find $(SRC_DIR)/$(HW_TEST_APP) -type f -iname '*.c' 2> /dev/null)
HW_TEST_OBJ = $(HW_TEST_SRC:.c=.o)

FPGA_INIT_APP = cetiFpgaInit
FPGA_INIT_SRC = $(shell find $(SRC_DIR)/$(FPGA_INIT_APP) -type f -iname '*.c' 2> /dev/null)
FPGA_INIT_OBJ = $(FPGA_INIT_SRC:.c=.o)

APPS = $(CAPTURE_APP) $(HW_TEST_APP) $(FPGA_INIT_APP)

CC         = gcc
CFLAGS     = -Wall -O2 -Wdate-time -D_FORTIFY_SOURCE=2 -Isrc
LDFLAGS    = -lpthread -lpigpio -lFLAC -lm

BINDIR    ?= bin
IPCDIR    ?= ipc
CONFIGDIR ?= config
DATADIR   ?= data
DESTDIR   ?= opt/ceti-tag-data-capture


.PHONY: \
	build \
	binary \
	install \
	clean \
	test \
	debug \
	$(APPS)
	
build: $(CAPTURE_APP) $(FPGA_INIT_APP)

debug: CFLAGS := -Wall -g -Wdate-time -D_FORTIFY_SOURCE=2 -DDEBUG 
debug: $(CAPTURE_APP)

binary: $(CAPTURE_APP)

$(BINDIR) :
	mkdir -p $@

$(APPS): % : $(BINDIR)/%

$(BINDIR)/$(CAPTURE_APP): $(CAPTURE_OBJ) $(HAL_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/$(HW_TEST_APP): $(HW_TEST_OBJ) $(HAL_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/$(FPGA_INIT_APP): $(FPGA_INIT_OBJ) $(HAL_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


install: $(BUILD_TARGETS)
	mkdir -p $(DESTDIR)
	cp -Rp $(BINDIR) $(DESTDIR)
	cp -Rp $(CONFIGDIR) $(DESTDIR)
	cp -Rp $(IPCDIR) $(DESTDIR)
	cp -Rp $(DATADIR) $(DESTDIR)

clean:
	rm -f $(HAL_OBJ) $(CAPTURE_OBJ) $(HW_TEST_OBJ) $(FPGA_INIT_OBJ)
	rm -rf $(BINDIR)
	rm -rf $(DESTDIR)
	$(MAKE) clean -C test

test:
	$(MAKE) test -C test

## -Ofast optimization used in trying to acheive higher audio data rates
# src/sensors/audio.o: CFLAGS := -Wall -Ofast -Wdate-time -D_FORTIFY_SOURCE=2