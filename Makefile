CC = gcc
CFLAGS = -Wall -Wpedantic
INCLUDEPATH = include
LINKPATH = link

all: lib
	$(CC) $(CFLAGS) test.c -o test -I$(INCLUDEPATH) -L$(LINKPATH) -lregexp9 -lfmt -lutf

lib:
	cd lib; ./lib.sh; cd ..;

.PHONY: all lib
