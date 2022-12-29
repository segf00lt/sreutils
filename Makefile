.POSIX:
CFLAGS = -Wall -Wpedantic -I./lib/include
LDFLAGS = -L./lib/link -lsre -lbio -lregexp9 -lfmt -lutf
DEBUG = -g -fsanitize=address

all: siv

install: test siv
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	cp -- siv   "$(DESTDIR)$(PREFIX)/bin"

siv: lib
	$(CC) $(CFLAGS) siv.c -o siv $(LDFLAGS)

test: lib
	$(CC) $(CFLAGS) siv.c -o siv_debug $(LDFLAGS)
	mv siv_debug test/siv_debug; cd test; ./test.sh; cd ..;

lib:
	cd lib; ./buildlib.sh; cd ..;

clean:
	rm -f siv *.o lib/link/* lib/include/*
	cd lib; ./buildlib.sh clean; cd ..

.PHONY: all install siv test lib clean