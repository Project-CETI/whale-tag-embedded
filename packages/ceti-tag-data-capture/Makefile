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

APPS = $(shell ls src/*/ -d | xargs basename -a)
APP_BIN = $(addprefix $(BINDIR)/, $(APPS))
APP_SRC = $(shell find $(SRC_DIR) -type f -iname '*.c' 2> /dev/null)
APP_OBJ = $(APP_SRC:.c=.o)


BINDIR    ?= bin
IPCDIR    ?= ipc
CONFIGDIR ?= config
DATADIR   ?= data
DESTDIR   ?= opt/ceti-tag-data-capture

### Tools ###
CC         = gcc
LD		   = ld

### Flags ###
CFLAGS     = -Wall -O2 -Wdate-time -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE
LDFLAGS    = -lpthread -lpigpio -lFLAC -lm -lrt

### Recipes ###

.PHONY: \
	build \
	binary \
	install \
	clean \
	debug \
	$(APPS)
	
build: $(APPS)

debug: CFLAGS := -Wall -g -Wdate-time -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -DDEBUG 
debug: $(APPS)

binary: $(APPS)

$(BINDIR):
	@mkdir -p $@

$(APPS): % : $(BINDIR)/%

#second expension required to filter with static pattern match
PERCENT = %
.SECONDEXPANSION:
$(APP_BIN): $(BINDIR)/% : $$(filter src/$$*/$$(PERCENT).o, $(APP_OBJ)) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: $(BUILD_TARGETS)
	mkdir -p $(DESTDIR)
	cp -Rp $(BINDIR) $(DESTDIR)
	cp -Rp $(CONFIGDIR) $(DESTDIR)
	cp -Rp $(IPCDIR) $(DESTDIR)
	cp -Rp $(DATADIR) $(DESTDIR)

clean:
	rm -f $(APP_OBJ)
	rm -rf $(BINDIR)
	rm -rf $(DESTDIR)
	@$(MAKE) test_clean

# See Test.mk for test related code
-include Test.mk 

## -Ofast optimization used in trying to acheive higher audio data rates
# src/sensors/audio.o: CFLAGS := -Wall -Ofast -Wdate-time -D_FORTIFY_SOURCE=2