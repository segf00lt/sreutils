
CFLAGS = -Wall -Werror -Wpedantic -g -I../include/

ARFLAGS = -rcs

all:
	$(CC) $(CFLAGS) -c sre.c -o sre.o
	$(AR) $(ARFLAGS) libsre.a sre.o

clean:
	rm -f libsre.a sre.o
