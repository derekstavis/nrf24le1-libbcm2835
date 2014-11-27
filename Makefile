PROJECT_NAME 	= nrf24le1
PROJECT_PATH	= $(pwd)

CC	    = gcc
INCLUDE	= -I/usr/local/include -I.
include Makefile.dev
CFLAGS	= -Wall $(INCLUDE) -Winline -pipe $(CFLAGSDEV) 

LDFLAGS	= -L/usr/local/lib
LIBS    = -l bcm2835 -l rt

SRC	    = main.c wiring.c nrf24le1.c
OBJ	    = $(addprefix $(OBJ_PATH)/, $(patsubst %.c,%.o,$(SRC)))
BINS	= main

BIN_PATH 	= bin
OBJ_PATH  	= obj

all: $(BIN_PATH)/$(PROJECT_NAME)

$(BIN_PATH)/nrf24le1: $(OBJ)
	@mkdir -p $(BIN_PATH)
	$(CC) -o $(BIN_PATH)/$(PROJECT_NAME) $(OBJ) $(LDFLAGS) $(LIBS)

$(OBJ_PATH)/%.o: %.c
	@mkdir -p $(OBJ_PATH)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_PATH)/wiring.o: wiring.c wiring.h
$(OBJ_PATH)/nrf24le1.o: wiring.h nrf24le1.h
$(OBJ_PATH)/main.o: nrf24le1.h

install:
	install -m 0777 $(BIN_PATH)/$(PROJECT_NAME) /usr/local/bin/$(PROJECT_NAME)

clean:
	rm -f $(OBJ_PATH)/* $(BIN_PATH)/*

.PHONY: all clean install
