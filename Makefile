# cc -o main main.c -I/usr/local/include -L/usr/local/lib -lwiringPi

#DEBUG	= -g -O0
DEBUG	= -O3
CC	= gcc
INCLUDE	= -I/usr/local/include -I.
#CFLAGS	= $(DEBUG) -Wall $(INCLUDE) -Winline -pipe

LDFLAGS	= -L/usr/local/lib
LIBS    = -lwiringPi

SRC	=	main.c wiring.c nrf24le1.c
OBJ	=	main.o
BINS	=	main

main:	
	@echo [CC]
	$(CC) -o $@ $(SRC) $(INCLUDE) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJ) *~ $(BINS)

tags:	$(SRC)
	@echo [ctags]
	@ctags $(SRC)

depend:
	makedepend -Y $(SRC)
