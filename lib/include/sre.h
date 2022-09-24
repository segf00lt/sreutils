#ifndef SRE_H
#define SRE_H

#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>

typedef struct {
	long s;
	long e;
} Bresub;

size_t Bgetre(Biobuf *bp, Reprog *progp, Resub *mp, int msize, Bresub *offp, char **wp, size_t *wsize);
int strgetre(char *str, Reprog *progp, Resub *mp, int msize);

#endif
