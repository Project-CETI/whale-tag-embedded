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

SRC        = $(shell find src -type f -iname '*.c' 2> /dev/null)
OBJ        = $(SRC:.c=.o)
CC         = gcc
CFLAGS     = -Wall -O2
LDFLAGS    = -lpthread -lpigpio -lFLAC -lm
APP        = cetiTagApp
HW_TEST    = cetiHWTestApp
FPGA_INIT_APP = cetiFpgaInit
BUILD_TARGETS = $(APP) $(HW_TEST) $(FPGA_INIT_APP)
APP_OBJ    = $(filter-out src/$(HW_TEST).o src/$(FPGA_INIT_APP).o, $(OBJ))
HW_TEST_OBJ = $(filter-out src/launcher.o src/commands.o src/$(FPGA_INIT_APP).o, $(OBJ))
FPGA_INIT_OBJ = src/fpga.o src/utils/logging.o src/$(FPGA_INIT_APP).o
BINDIR    ?= bin
IPCDIR    ?= ipc
CONFIGDIR ?= config
DATADIR   ?= data
DESTDIR   ?= opt/ceti-tag-data-capture


$(APP): $(APP_OBJ)
.PHONY: \
	build \
	binary \
	install \
	clean \
	test \
	debug
	
build: $(BUILD_TARGETS)

debug: CFLAGS := -Wall -g -DDEBUG
debug: $(APP) $(HW_TEST)

binary: $(APP)

$(APP): $(APP_OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(APP) $^ $(LDFLAGS)

$(HW_TEST): $(HW_TEST_OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(HW_TEST) $^ $(LDFLAGS)

$(FPGA_INIT_APP): $(FPGA_INIT_OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(FPGA_INIT_APP) $^ $(LDFLAGS)

install: $(BUILD_TARGETS)
	mkdir -p $(DESTDIR)
	cp -Rp $(BINDIR) $(DESTDIR)
	cp -Rp $(CONFIGDIR) $(DESTDIR)
	cp -Rp $(IPCDIR) $(DESTDIR)
	cp -Rp $(DATADIR) $(DESTDIR)

clean:
	rm -f $(OBJ)
	rm -rf $(BINDIR)
	rm -rf $(DESTDIR)
	$(MAKE) clean -C test

test:
	$(MAKE) test -C test
