CC = gcc
CFLAGS = -Wall -Wpedantic
DEBUG = $(CFLAGS) -g -fsanitize=address
INCLUDEPATH = lib/include
LINKPATH = lib/link

all: lib test siv

siv: lib
	$(CC) $(CFLAGS) -g siv.c structregex.c \
		-o siv \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf

test:
	$(CC) $(DEBUG) siv.c structregex.c \
		-o siv_debug \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf
	mv siv_debug test/bin; cd test; ./test.sh; cd ..;

lib:
	cd lib; ./buildlib.sh; cd ..;

.PHONY: all siv test lib
