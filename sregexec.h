#ifndef _SREREGEXEC_H
#define _SREREGEXEC_H

#define MAXSUBEXP 10

typedef struct Sresub Sresub;

struct Sresub {
	long s;
	long e; /* end is non inclusive */
};

typedef struct Sresublist Sresublist;

struct Sresublist {
	Sresub m[MAXSUBEXP];
};

typedef struct Srelist Srelist;

struct Srelist {
	Reinst *inst;
	Sresublist se;
};

int sregexec(Reprog *progp, Biobuf *bp, Sresub *mp, int ms);

#endif
