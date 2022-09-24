CC = gcc
CFLAGS = -Wall -Wpedantic -g
DEBUG = $(CFLAGS) -g -fsanitize=address
INCLUDEPATH = lib/include
LINKPATH = lib/link

all: lib test siv

siv: lib
	$(CC) $(CFLAGS) siv.c \
		-o siv \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lsre -lbio -lregexp9 -lfmt -lutf

test:
	$(CC) $(DEBUG) siv.c \
		-o siv_debug \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lsre -lbio -lregexp9 -lfmt -lutf
	mv siv_debug test/siv_debug; cd test; ./test.sh; cd ..;

lib:
	cd lib; ./buildlib.sh; cd ..;

.PHONY: all siv test lib
