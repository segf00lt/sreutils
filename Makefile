CC = gcc
CFLAGS = -Wall -Wpedantic -g
INCLUDEPATH = lib/include
LINKPATH = lib/link

siv: lib
	$(MAKE) -C siv/ all

test: lib
	$(CC) $(CFLAGS) test.c -o test -I$(INCLUDEPATH) -L$(LINKPATH) -lbio -lregexp9 -lfmt -lutf

lib:
	cd lib; ./lib.sh; cd ..;

.PHONY: siv test lib
