PROJECT_NAME 	= nrf24le1
PROJECT_PATH	= $(pwd)

#DEBUG	= -g -O0
DEBUG	= -O3
CC	    = gcc
INCLUDE	= -I/usr/local/include -I.
#CFLAGS	= $(DEBUG) -Wall $(INCLUDE) -Winline -pipe

LDFLAGS	= -L/usr/local/lib
LIBS    = -l bcm2835 -l rt

SRC	    = main.c wiring.c nrf24le1.c
OBJ	    = main.o
BINS	= main

BIN_PATH 	= bin
OBJ_PATH  	= obj
DIST_PATH 	= dist

main:	
	@echo [CC]
	$(CC) -o $(BIN_PATH)/$(PROJECT_NAME) $(SRC) $(INCLUDE) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJ_PATH)/* $(BIN_PATH)/*