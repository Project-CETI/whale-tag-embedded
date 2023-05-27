#-----------------------------------------------------------------------------
# Description
# Pressure Logger Makefile
#   * derived from cetiTagApp makefile (see v2_2_refactor branch)
#-----------------------------------------------------------------------------

SRC        = $(wildcard src/*.c) $(wildcard src/*/*.c) $(wildcard src/*/*/*.c)
OBJ        = $(SRC:.c=.o)
CC         = gcc
CFLAGS     = -Wall -g
LDFLAGS    = -lpthread -lpigpio -lFLAC -lm
APP        = pressureLogger
BINDIR    ?= bin
IPCDIR    ?= ipc
DATADIR   ?= data
DESTDIR   ?= opt/pressure-logger

build: $(APP)

$(APP): $(OBJ)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/$(APP) $(OBJ) $(LDFLAGS)

install: build
	mkdir -p $(DESTDIR)
	cp -Rp $(BINDIR) $(DESTDIR)
	cp -Rp $(IPCDIR) $(DESTDIR)
	cp -Rp $(DATADIR) $(DESTDIR)

clean:
	rm -f $(OBJ)
	rm -rf $(BINDIR)
	rm -rf $(DESTDIR)
