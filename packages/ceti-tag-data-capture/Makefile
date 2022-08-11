#-----------------------------------------------------------------------------
# CETI Tag Electronics
# Cummings Electronics Labs, October 2021
# Developed under contract for Harvard University Wood Lab
#-----------------------------------------------------------------------------
# Version		Date		Description
#	0.00    10/08/21   Begin work, establish framework
#       2.00    12/06/21   Port over to github.com/Project-CETI/whale-tag-embedded, add to debian build
#       2.1.0   03/15/22   Target the summer 2022 deployment based on hardware v2.1
#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------
# Description
# This is the CETI makefile for the top level
#-----------------------------------------------------------------------------

SRC        = $(wildcard src/*.c) $(wildcard src/*/*.c)
OBJ        = $(SRC:.c=.o)
CC         = gcc
CFLAGS     = -Wall -g
LDFLAGS    = -lpthread -lpigpio
APP        = cetiTagApp
BINDIR    ?= bin
IPCDIR    ?= ipc
CONFIGDIR ?= config
DATADIR   ?= data
LOGDIR    ?= log
DESTDIR   ?= opt/ceti-tag-data-capture

build: $(APP)

$(APP): $(OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(APP) $(LDFLAGS) $(OBJ)

install: build
	mkdir -p $(DESTDIR)
	mkdir -p $(DESTDIR)/$(LOGDIR)
	cp -Rp $(BINDIR) $(DESTDIR)
	cp -Rp $(CONFIGDIR) $(DESTDIR)
	cp -Rp $(IPCDIR) $(DESTDIR)
	cp -Rp $(DATADIR) $(DESTDIR)

clean:
	rm -f $(OBJ)
	rm -rf $(BINDIR)
	rm -rf $(DESTDIR)