.POSIX:
CFLAGS = -Wall -Wpedantic -I./lib/include
LDFLAGS = -L./lib/link -lsre -lbio -lregexp9 -lfmt -lutf
DEBUG = -g -fsanitize=address

all: siv

install: test siv
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	cp -- siv   "$(DESTDIR)$(PREFIX)/bin"

siv: lib
	$(CC) $(CFLAGS) $(LDFLAGS) siv.c -o siv

test: lib
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEBUG) siv.c -o siv_debug
	mv siv_debug test/siv_debug; cd test; ./test.sh; cd ..;

lib:
	cd lib; ./buildlib.sh; cd ..;

clean:
	rm -f siv *.o lib/link/* lib/include/*
	cd lib; ./buildlib.sh clean; cd ..

.PHONY: all install siv test lib clean
