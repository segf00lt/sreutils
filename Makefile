CC = gcc
CFLAGS = -Wall -Wpedantic -g
INCLUDEPATH = lib/include
LINKPATH = lib/link

siv: lib
	$(CC) $(CFLAGS) siv.c -o siv \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf

test: test_Bgetre_comment test_Bgetre_cfunc test_Bgetre_pipe
	cd test; ./unittest.sh; cd ..;

test_Bgetre_comment: lib
	$(CC) $(CFLAGS) -fsanitize=address test/test_Bgetre_comment.c structregex.c \
		-o test/test_Bgetre_comment \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf

test_Bgetre_cfunc: lib
	$(CC) $(CFLAGS) test/test_Bgetre_cfunc.c structregex.c \
		-o test/test_Bgetre_cfunc \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf

test_Bgetre_pipe: lib
	$(CC) $(CFLAGS) test/test_Bgetre_pipe.c structregex.c \
		-o test/test_Bgetre_pipe \
		-I$(INCLUDEPATH) -L$(LINKPATH) \
		-lbio -lregexp9 -lfmt -lutf

lib:
	cd lib; ./lib.sh; cd ..;

.PHONY: siv test test_Bgetre_comment test_Bgetre_cfunc test test_Bgetre_pipe lib
