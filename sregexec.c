#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <regcomp.h>
#include <bio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define STATICLEN 10
#define DYNLEN 250

typedef struct Sresub {
	unsigned long s;
	unsigned long e;
} Sresub;

typedef struct Sresublist {
	Sresub m[NSUBEXP]; /* NSUBEXP defined in lib/libregexp/regcomp.h */
} Sresublist;

typedef struct Srelist {
	Reinst *inst;
	Sresublist se;
} Srelist;

size_t
Bfsize(Biobuf *bp)
{
	int fd = Bfildes(bp);
	struct stat s;
	fstat(fd, &s);
	return s.st_size;
}

static int
inclass(Rune r, /* character to match */
	Reclass* c) /* character class */
{
	Rune *rp = c->spans;
	Rune *ep = c->end;

	for(; rp < ep; rp += 2) {
		if(r >= rp[0] && r <= rp[1])
			return 1;
	}

	return 0;
}

/*
 * Note optimization in addthread (taken from _renewthread):
 * 	*lp must be pending when _renewthread called; if *l has been looked
 *		at already, the optimization is a bug.
 */
static Srelist*
addthread(Srelist *lp,	/* list to add to */
	Reinst *ip,	/* instruction to add */
	int ms,
	Sresublist *sub)/* subexpressions */
{
	Srelist *p;

	for(p = lp; p->inst; ++p) {
		if(p->inst == ip) {
			if(sub->m[0].s < p->se.m[0].s) {
				if(ms > 1)
					p->se = *sub;
				else
					p->se.m[0] = sub->m[0];
			}
			return 0;
		}
	}
	p->inst = ip;
	if(ms > 1)
		p->se = *sub;
	else
		p->se.m[0] = sub->m[0];
	(++p)->inst = 0;
	return p;
}

/* structural regex execution is performed on a file (or Biobuf in this case)
 * as opposed to an in memory string
 */
int
sregexec(Biobuf *bp,	/* file buffer to read */
	 Reprog *progp,	/* regex prog to execute */
	 Sresub *mp,	/* subexpression elements */
	 int ms)	/* num of elem in mp */
{
	Relist threadlist0[STATICLEN], threadlist1[STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;

	Srelist *tl = threadlist0;
	Srelist *nl = threadlist1;
	Srelist *tle = threadlist0 + STATICLEN - 2;
	Srelist *nle = threadlist1 + STATICLEN - 2;
	Srelist *tmp = NULL;

	Srelist *tlp;
	Reinst *inst;

	Sresublist sl; /* used for reinitializing submatches */

	unsigned long start;
	unsigned long end;
	unsigned long pos;

	if(mp && ms > 0) {
		if(mp->s)
			start = mp->s;
		if(mp->e)
			end = mp->e;

		for(int i = 0; i < ms; ++i)
			mp[i].s = mp[i].e = 0;
	}

	int starttype = progp->startinst->type;
	Rune startchar = starttype == RUNE ? progp->startinst->u1.r : 0;

	int overflow = 0; /* counts number of times tl and/or nl's capacity is reached */
	int match = 0;
	int checkstart = 0;

	Bseek(bp, start, 0); /* go to start */

Overflow:
	if(overflow++ == 1) {
		tmp = malloc(DYNLEN * sizeof(Srelist));
		memcpy(tmp, tl, tle - tl);
		tl = tmp;

		tmp = malloc(DYNLEN * sizeof(Srelist));
		memcpy(tmp, nl, nle - nl);
		nl = tmp;

		tle = tl + DYNLEN - 2;
		nle = nl + DYNLEN - 2;
	} else if(overflow > 1) {
		if(tl)
			free(tl);
		if(nl)
			free(nl);

		fprint(2, "sregexec: overflowed threadlist\n");

		return -1;
	}

	for(Rune r = Bgetrune(bp), prevr = 0; (pos = Boffset(bp)) <= end; prevr = r, r = Bgetrune(bp)) {

		if(startchar && nl->inst == 0 && startchar != r) /* skip to first character in progp */
			continue;

		/* swap lists */
		tmp = tl;
		tl = nl;
		nl = tmp;
		tmp = tle;
		tle = nle;
		nle = tmp;

		if(match == 0 && tl->inst == 0) { /* restart until progress is made or match is found */
			sl.m[0].s = pos;
			addthread(tl, progp->startinst, ms, &sl);
		}

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
				switch(inst->type){
				case RUNE:
					if(inst->u1.r == r) {
						if(addthread(nl, inst->u2.next, ms, &tlp->se) == nle)
							goto Overflow;
					}
					break;
				case LBRA:
					tlp->se.m[inst->u1.subid].s.sp = s;
					continue;
				case RBRA:
					tlp->se.m[inst->u1.subid].e.ep = s;
					continue;
				case ANY:
					if(r != '\n') {
						if(addthread(nl, inst->u2.next, ms, &tlp->se) == nle)
								goto Overflow;
					}
					break;
				case ANYNL:
					if(addthread(nl, inst->u2.next, ms, &tlp->se) == nle)
						goto Overflow;
					break;
				case BOL:
					if(pos == start || prevr == '\n')
						continue;
					break;
				case EOL:
					if(pos == end || r == 0 || r == '\n')
						continue;
					break;
				case CCLASS:
					if(inclass(r, inst->u1.cp)) {
						if(addthread(nl, inst->u2.next, ms, &tlp->se) == nle)
							goto Overflow;
					}
					break;
				case NCCLASS:
					if(!inclass(r, inst->u1.cp)) {
						if(addthread(nl, inst->u2.next, ms, &tlp->se) == nle)
							goto Overflow;
					}
					break;
				case OR:
					/* evaluate right choice later */
					if(addthread(tlp, inst->u1.right, ms, &tlp->se) == tle)
						goto Overflow;
					/* efficiency: advance and re-evaluate */
					continue;
				case END:	/* Match! */
					match = 1;
					tlp->se.m[0].e = pos;
					if(mp != 0)
						savematch(mp, ms, &tlp->se);
					break;
				}
				break;
			} /* inner thread loop */
		} /* outer thread loop */

		if(pos == end)
			break;

		checkstart = startchar && nl->inst == 0;
	} /* file read loop */

	if(overflow) {
		free(tl);
		free(nl);
	}

	return match;
}
