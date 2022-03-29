CC = gcc
CFLAGS = -Wall -Wpedantic -g
INCLUDEPATH = lib/include
LINKPATH = lib/link

all: lib
	$(CC) $(CFLAGS) test.c -o test -I$(INCLUDEPATH) -L$(LINKPATH) -lbio -lregexp9 -lfmt -lutf

lib:
	cd lib; ./lib.sh; cd ..;

.PHONY: all lib
