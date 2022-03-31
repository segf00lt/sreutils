#ifndef _SREREGEXEC_H
#define _SREREGEXEC_H

#define MAXSUBEXP 10

typedef struct Sresub {
	unsigned long s;
	unsigned long e;
} Sresub;

typedef struct Sresublist {
	Sresub m[MAXSUBEXP];
} Sresublist;

typedef struct Srelist {
	Reinst *inst;
	Sresublist se;
} Srelist;

int sregexec(Reprog *progp, Biobuf *bp, Sresub *mp, int ms);

#endif
