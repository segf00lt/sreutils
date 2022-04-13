CC = gcc
CFLAGS = -Wall -Wpedantic -g
INCLUDEPATH = lib/include
LINKPATH = lib/link

siv: lib
	$(CC) $(CFLAGS) siv.c -o siv -I$(INCLUDEPATH) -L$(LINKPATH) -lbio -lregexp9 -lfmt -lutf

test: lib
	$(CC) $(CFLAGS) test.c -o test -I$(INCLUDEPATH) -L$(LINKPATH) -lbio -lregexp9 -lfmt -lutf

lib:
	cd lib; ./lib.sh; cd ..;

.PHONY: siv test lib
