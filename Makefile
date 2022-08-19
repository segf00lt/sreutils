CC = gcc
CFLAGS = -Wall -Wpedantic -g
INCLUDEPATH = lib/include
LINKPATH = lib/link

siv: lib
	$(CC) $(CFLAGS) siv.c \
		-o siv \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf

test:
	cd test; ./test.sh; cd ..;

lib:
	cd lib; ./buildlib.sh; cd ..;

.PHONY: siv test lib
