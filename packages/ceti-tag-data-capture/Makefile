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
APP_OBJ    = $(filter-out src/$(HW_TEST).o, $(OBJ))
HW_TEST_OBJ = $(filter-out src/launcher.o src/commands.o, $(OBJ))
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
	
build: $(APP) $(HW_TEST)

debug: CFLAGS := -Wall -g -DDEBUG
debug: $(APP) $(HW_TEST)

binary: $(APP)

$(APP): $(OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(APP) $(APP_OBJ) $(LDFLAGS)

$(HW_TEST): $(HW_TEST_OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(HW_TEST) $(HW_TEST_OBJ) $(LDFLAGS)

install: build
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
