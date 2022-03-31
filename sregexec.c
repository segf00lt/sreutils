#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <regcomp.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sregexec.h"

#define STATICLEN 11
#define DYNLEN 301

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

static void
savematch(Sresub *mp,
	int ms,
	Sresublist *sp)
{
	int i;

	if(mp == 0 || ms <= 0)
		return;

	if( mp[0].s == 0 || sp->m[0].s < mp[0].s || (sp->m[0].s == mp[0].s && sp->m[0].e > mp[0].e) ) {
		for(i = 0; i < ms && i < NSUBEXP; i++)
			mp[i] = sp->m[i];
		for(; i < ms; i++)
			mp[i].s = mp[i].e = 0;
	}
}

/* structural regex execution is performed on a file (or Biobuf in this case)
 * as opposed to an in memory string
 */
int
sregexec(Reprog *progp,	/* regex prog to execute */
	Biobuf *bp,	/* file buffer to read */
	Sresub *mp,	/* subexpression elements */
	int ms)	/* num of elem in mp */
{
	Srelist threadlist0[STATICLEN], threadlist1[STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;

	Srelist *tl = threadlist0;
	Srelist *nl = threadlist1;
	Srelist *tle = threadlist0 + STATICLEN - 2;
	Srelist *nle = threadlist1 + STATICLEN - 2;
	Srelist *tmp = NULL;

	Srelist *tlp;
	Reinst *inst;

	Sresublist sl; /* used for reinitializing submatches */

	long start;
	long end;
	long pos;

	Rune r;
	Rune prevr;

	if(mp && ms > 0) {
		start = mp->s;
		end = mp->e;

		for(int i = 0; i < ms; ++i)
			mp[i].s = mp[i].e = 0;
	} else {
		start = 0;
		end = Bseek(bp, 0, SEEK_END);
	}

	int starttype = progp->startinst->type;
	Rune startchar = starttype == RUNE ? progp->startinst->u1.r : 0;

	int overflow = 0; /* counts number of times tl and/or nl's capacity is reached */
	int match = 0;

	Bseek(bp, start, 0); /* go to start */

Execloop:
	while((pos = Boffset(bp)) < end) {

		prevr = r;
		r = Bgetrune(bp);

		if(startchar && nl->inst == 0 && startchar != r) /* skip to first character in progp */
			continue;

		/* swap lists */
		tmp = tl;
		tl = nl;
		nl = tmp;
		tmp = tle;
		tle = nle;
		nle = tmp;
		nl->inst = 0;

		if(match == 0 && tl->inst == 0) { /* restart until progress is made or match is found */
			sl.m[0].s = pos;
			addthread(tl, progp->startinst, ms, &sl);
		}

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
				switch(inst->type) {
					case RUNE:
						if(inst->u1.r == r) {
							if(addthread(nl, inst->u2.next, ms, &tlp->se) == nle)
								goto Overflow;
						}
						break;
					case LBRA:
						tlp->se.m[inst->u1.subid].s = pos;
						continue;
					case RBRA:
						tlp->se.m[inst->u1.subid].e = pos;
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
					/* TODO: make pattern ^$ work for matching empty lines */
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
	} /* file read loop */

	if(overflow) {
		free(tl);
		free(nl);
	}

	return match;

Overflow:
	if(++overflow == 1) {
		tmp = calloc(DYNLEN, sizeof(Srelist));
		memcpy(tmp, tl, tle - tl);
		for(int i = 0; i < MAXSUBEXP; ++i)
			tmp->se.m[i] = tl->se.m[i];
		tl = tmp;

		tmp = calloc(DYNLEN, sizeof(Srelist));
		memcpy(tmp, nl, nle - nl);
		for(int i = 0; i < MAXSUBEXP; ++i)
			tmp->se.m[i] = nl->se.m[i];
		nl = tmp;

		tle = tl + DYNLEN - 2;
		nle = nl + DYNLEN - 2;

		goto Execloop;
	}

	/* if overflowed twice exit */
	if(tl)
		free(tl);
	if(nl)
		free(nl);

	fprint(2, "sregexec: overflowed threadlist\n");

	return -1;
}
