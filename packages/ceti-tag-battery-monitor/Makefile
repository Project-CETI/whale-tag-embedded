#-----------------------------------------------------------------------------
# CETI Tag Electronics
# Cummings Electronics Labs, October 2021
# Developed under contract for Harvard University Wood Lab
#-----------------------------------------------------------------------------
# Version	Date		Description
#	0.00    01/31/23	Create a separate C application to query gas gauge
#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------
# Description
# This is the CETI makefile for the top level
#-----------------------------------------------------------------------------

SRC        = $(wildcard src/*.c)
OBJ        = $(SRC:.c=.o)
CC         = gcc
CFLAGS     = -Wall -g
LDFLAGS    = -lpigpio
APP        = cetiTagBatteryStatus
BINDIR    ?= bin
DESTDIR   ?= opt/ceti-tag-battery-monitor

build: $(APP)

$(APP): $(OBJ)
	mkdir -p $(BINDIR)
	cp -Rp src/batterymon.sh $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(APP) $(LDFLAGS) $(OBJ)

install: build
	mkdir -p $(DESTDIR)
	cp -Rp $(BINDIR) $(DESTDIR)

clean:
	rm -f $(OBJ)
	rm -rf $(BINDIR)
	rm -rf $(DESTDIR)