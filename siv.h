#ifndef _SIV_H
#define _SIV_H

#define REMAX 10

typedef struct {
	Sresub *p;
	size_t l;
	size_t c;
} Sresubarr;

typedef struct {
	int fd;
	char *name;
	Sresubarr data[REMAX];
} Yield;

#endif
