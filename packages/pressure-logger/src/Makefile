#-----------------------------------------------------------------------------
# CETI Tag Electronics
# Dual Pressure Sensor Data Logger 
# 
#-----------------------------------------------------------------------------
# File: Makefile
#
# Makefile for the program. See pressureLogger.h for additional notes
#-----------------------------------------------------------------------------

SRC     = $(wildcard *.c)
OBJ     = $(SRC:.c=.o)
CC      = gcc
CFLAGS  = -Wall -g
LDFLAGS = -lpthread -pthread -lpigpio -li2c
OUT     = dualPressureLogger
OUTDIR  = ../bin

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUTDIR)/$(OUT) $(LDFLAGS) $(OBJ)

clean:
	rm -f $(OBJ) $(OUTDIR)/$(OUT)