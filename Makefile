CC = gcc
CFLAGS = -Wall -Wpedantic -g
DEBUG = $(CFLAGS) -g -fsanitize=address
INCLUDEPATH = lib/include
LINKPATH = lib/link
INSTALLPATH = /usr/local/bin

all: lib test siv

install: test siv
	cp -f siv $(INSTALLPATH)
	chmod 755 $(INSTALLPATH)/siv

siv: lib
	$(CC) $(CFLAGS) siv.c \
		-o siv \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lsre -lbio -lregexp9 -lfmt -lutf

test: lib
	$(CC) $(DEBUG) siv.c \
		-o siv_debug \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lsre -lbio -lregexp9 -lfmt -lutf
	mv siv_debug test/siv_debug; cd test; ./test.sh; cd ..;

lib:
	cd lib; ./buildlib.sh; cd ..;

clean:
	rm -f siv *.o lib/link/* lib/include/*
	cd lib; ./buildlib.sh clean; cd ..

.PHONY: all siv test lib install clean
