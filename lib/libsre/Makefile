CC = gcc
CFLAGS = -Wall -Werror -Wpedantic -g
AR = ar
ARFLAGS = -rcs
INCLUDEPATH = ../include/

all:
	$(CC) $(CFLAGS) -c sre.c -o sre.o -I$(INCLUDEPATH)
	$(AR) -rcs libsre.a sre.o

clean:
	rm -f libsre.a sre.o
