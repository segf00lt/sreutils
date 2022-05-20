CC = gcc
CFLAGS = -g -Wall -Wpedantic
INCLUDEPATH = ../lib/include
LINKPATH = ../lib/link

all:
	$(CC) $(CFLAGS) siv.c -o siv -I$(INCLUDEPATH) -L$(LINKPATH) -lregexp9 -lfmt -lutf
.PHONY: all
