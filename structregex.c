/*
 * structural regular expressions
 *
 * Bgetre() extracts a regular expression from a file
 * storing the extraction, and the capacity of
 * the buffer that stores it, in variables provided
 * by the user
 *
 * the return value of Bgetre() is the length of the
 * extraction 
 *
 * strgetre() extracts a regular expression from a char *
 * storing the beginning and end of the extraction in a
 * Resub *
 *
 * if the Resub *mp holds a range and int msize is greater
 * than 0, extraction will happen between mp.s.sp and mp.e.ep
 *
 * the return value of strgetre() is 1 for success and 0
 * for failure
 */

#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <regcomp.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define _STATICLEN 11
#define _DYNLEN 151
#define _MAXSUBEXP 10

/* TODO compilers */

/* executors */
static int
inclass(Rune r, /* character to match */
	Reclass* c) /* character class */
{
	Rune *rp = c->spans;
	Rune *ep = c->end;

	for(; rp < ep; rp += 2)
		if(r >= rp[0] && r <= rp[1])
			return 1;

	return 0;
}

static Relist*
addthread(Relist *lp,	/* list to add to */
	Reinst *ip,	/* instruction to add */
	int ms,
	Resublist *sub)/* subexpressions */
{
	Relist *p;

	for(p = lp; p->inst; ++p) {
		if(p->inst == ip) {
			if(sub->m[0].s.sp < p->se.m[0].s.sp) {
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
savematch(Resub *mp, int ms, Resublist *sp)
{
	int i;

	if(mp == 0 || ms <= 0)
		return;

	/* NOTE
	 * A match will be saved iff
	 * the beginning of mp group 0 is NULL
	 * or the current thread's group 0 starts before mp group 0
	 * or
	 * the current thread's group 0 starts in the same place as mp group 0
	 * and
	 * the current thread's group 0 ends further in the string than mp group 0
	 */
	if(mp[0].s.sp == 0 || sp->m[0].s.sp < mp[0].s.sp ||
		(sp->m[0].s.sp == mp[0].s.sp && sp->m[0].e.ep > mp[0].e.ep))
	{
		for(i = 0; i < ms && i < NSUBEXP; i++)
			mp[i] = sp->m[i];

		for(; i < ms; i++)
			mp[i].s.sp = mp[i].e.ep = 0;
	}
}

size_t
Bgetre(Biobuf *bp, /* file to read */
	Reprog *progp, /* regexp used */
	Resub *mp, /* submatch array */
	int msize, /* mp capacity */
	long *offstart, /* offset for start of match */
	long *offend, /* offset for end of match */
	char **wp, /* addr of buffer to store main match */
	size_t *wsize) /* addr of var to store size of wp */
{
	Relist threadlist0[_STATICLEN];
	Relist threadlist1[_STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;

	Relist *tl = threadlist0;
	Relist *nl = threadlist1;
	Relist *tle = threadlist0 + _STATICLEN - 2;
	Relist *nle = threadlist1 + _STATICLEN - 2;
	Relist *tmp = 0;

	Relist *tlp;
	Reinst *inst;

	Resublist sl;

	long start;
	long end;

	Rune r;
	Rune prevr;
	r = prevr = 0;

	int starttype = progp->startinst->type;
	Rune startchar = (starttype == RUNE) * progp->startinst->u1.r;

	int overflow = 0;
	int match = 0;
	int greedy = 0;
	char *endgreed;

	char *s = *wp;
	size_t size = *wsize;
	size_t i = 0; /* position in s */

	if(bp->flag == Bmagic || bp->bbuf == bp->b) {
		fprint(2, "Bgetre: Biobuf must be created with Binits()\n");
		exit(1);
	}

	if(msize > 0 && mp != 0) {
		for(int i = 0; i < msize; ++i) { /* VERY IMPORTANT savematch() won't work otherwise */
			mp[i].s.sp = 0;
			mp[i].e.ep = 0;
		}
	}

Bgetre_Execloop:
	while(r != Beof) {
		prevr = r;
		r = Bgetrune(bp);

		/* skip to first character in progp */
		if(startchar && nl->inst == 0 && startchar != r && r != Beof) {
			i = 0; /* go back to beginning of s */
			continue;
		}

		/* swap lists */
		tmp = tl;
		tl = nl;
		nl = tmp;
		tmp = tle;
		tle = nle;
		nle = tmp;
		nl->inst = 0;

		if(match == 0 && tl->inst == 0) { /* restart until progress is made or match is found */
			i = 0;
			sl.m[0].s.sp = s;
			start = Boffset(bp);
			addthread(tl, progp->startinst, msize, &sl);
		} else
			++i; /* if matching step position in s */

		if(i >= size) { /* realloc s if matching */
			size <<= 1;
			s = realloc(s, size);
		}

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
				switch(inst->type) {
					case RUNE:
						if(inst->u1.r == r) {
					Bgetre_Addthreadnext:
							s[i] = r; /* save r in s */
							if(addthread(nl, inst->u2.next, msize, &tlp->se) == nle)
								goto Bgetre_Overflow;
						} else
							greedy ^= inst->u2.next->greedy;
						break;
					case LBRA:
						tlp->se.m[inst->u1.subid].s.sp = s;
						continue;
					case RBRA:
						tlp->se.m[inst->u1.subid].e.ep = s;
						continue;
					case ANY:
						if(r != '\n')
							goto Bgetre_Addthreadnext;
						greedy ^= inst->u2.next->greedy;
						break;
					case ANYNL:
						goto Bgetre_Addthreadnext;
						break;
					case BOL:
						if(prevr == 0 || prevr == '\n')
							continue;
						break;
					case EOL:
						if(r == Beof || r == 0 || r == '\n')
							goto Bgetre_Addthreadnext;
						break;
					case CCLASS:
						if(inclass(r, inst->u1.cp))
							goto Bgetre_Addthreadnext;
						greedy ^= inst->u2.next->greedy;
						break;
					case NCCLASS:
						if(!inclass(r, inst->u1.cp))
							goto Bgetre_Addthreadnext;
						greedy ^= inst->u2.next->greedy;
						break;
					case OR:
						greedy = inst->greedy;

						/* evaluate right choice later */
						if(addthread(tlp, inst->u1.right, msize, &tlp->se) == tle)
							goto Bgetre_Overflow;
						/* efficiency: advance and re-evaluate */
						continue;
					case END: /* Match! */
						match = 1;
						tlp->se.m[0].e.ep = s + i;
						endgreed = s + i;
						end = Boffset(bp) - 1;
						if(mp)
							savematch(mp, msize, &tlp->se);
						if(!greedy)
							goto Bgetre_Return; /* nongreedy */
				}

				break; /* IMPORTANT BREAK */

			} /* inner thread loop */
		} /* outer thread loop */
	} /* file read loop */

Bgetre_Return:
	if(overflow) {
		free(tl);
		free(nl);
	}

	if(greedy) {
		register size_t n = s + i - endgreed;

		if(n >= bp->bsize) {
			register size_t newsize = n + Bungetsize;
			/* round newsize up to closest power of 2 */
			--newsize;
			newsize |= (newsize >> 1);
			newsize |= (newsize >> 2);
			newsize |= (newsize >> 4);
			newsize |= (newsize >> 8);
			newsize |= (newsize >> 16);
			newsize |= (newsize >> 32);
			++newsize;
			bp->bsize = newsize;
			bp->bbuf = realloc(bp->bbuf - Bungetsize, bp->bsize);
			bp->ebuf = bp->bbuf + bp->bsize;
			bp->bbuf += Bungetsize; /* IMPORTANT */
		}

		bp->icount = -n;
		bp->gbuf = bp->ebuf + bp->icount;
		memcpy(bp->gbuf, endgreed, n);
		i -= n;
	}

	/* writeback s and size */
	if(offstart)
		*offstart = start;
	if(offend)
		*offend = end;
	s[i] = 0;
	*wp = s;
	*wsize = size;

	return i; /* return length of s */

Bgetre_Overflow:
	if(++overflow == 1) {
		tmp = calloc(_DYNLEN, sizeof(Relist));
		memcpy(tmp, tl, tle - tl);
		for(int i = 0; i < _MAXSUBEXP; ++i)
			tmp->se.m[i] = tl->se.m[i];
		tl = tmp;

		tmp = calloc(_DYNLEN, sizeof(Relist));
		memcpy(tmp, nl, nle - nl);
		for(int i = 0; i < _MAXSUBEXP; ++i)
			tmp->se.m[i] = nl->se.m[i];
		nl = tmp;

		tle = tl + _DYNLEN - 2;
		nle = nl + _DYNLEN - 2;

		goto Bgetre_Execloop;
	}

	/* if overflowed twice exit */
	if(tl)
		free(tl);
	if(nl)
		free(nl);

	fprint(2, "sregexec: overflowed threadlist\n");

	return -1;
}

int
strgetre(char *str, /* string to read */
	Reprog *progp, /* regexp used */
	Resub *mp, /* submatch array */
	int msize) /* mp capacity */
{
	Relist threadlist0[_STATICLEN];
	Relist threadlist1[_STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;

	Relist *tl = threadlist0;
	Relist *nl = threadlist1;
	Relist *tle = threadlist0 + _STATICLEN - 2;
	Relist *nle = threadlist1 + _STATICLEN - 2;
	Relist *tmp = 0;

	Relist *tlp;
	Reinst *inst;

	Resublist sl;

	Rune r;
	Rune prevr;
	r = prevr = 0;

	int starttype = progp->startinst->type;
	Rune startchar = (starttype == RUNE) * progp->startinst->u1.r;

	int overflow = 0;
	int match = 0;
	int greedy = 0;

	char *curp, *nextp;
	char *begin;
	char *end;

	begin = str;
	end = 0;

	if(msize > 0 && mp != 0) {
		if(mp->s.sp)
			begin = mp->s.sp;
		if(mp->e.ep)
			end = mp->e.ep;

		for(int i = 0; i < msize; ++i) { /* VERY IMPORTANT savematch() won't work otherwise */
			mp[i].s.sp = 0;
			mp[i].e.ep = 0;
		}
	}

	nextp = curp = begin;

strgetre_Execloop:
	do {
		prevr = r;
		nextp += chartorune(&r, curp);

		/* skip to first character in progp */
		if(startchar && nl->inst == 0 && startchar != r && r != 0) {
			if(curp == end)
				break;

			curp = nextp;
			continue;
		}

		/* swap lists */
		tmp = tl;
		tl = nl;
		nl = tmp;
		tmp = tle;
		tle = nle;
		nle = tmp;
		nl->inst = 0;

		if(match == 0 && tl->inst == 0) { /* restart until progress is made or match is found */
			sl.m[0].s.sp = curp;
			addthread(tl, progp->startinst, msize, &sl);
		}

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
				switch(inst->type) {
					case RUNE:
						if(inst->u1.r == r) {
					strgetre_Addthreadnext:
							if(addthread(nl, inst->u2.next, msize, &tlp->se) == nle)
								goto strgetre_Overflow;
						} else
							greedy ^= inst->u2.next->greedy;
						break;
					case LBRA:
						tlp->se.m[inst->u1.subid].s.sp = curp;
						continue;
					case RBRA:
						tlp->se.m[inst->u1.subid].e.ep = curp;
						continue;
					case ANY:
						if(r != '\n')
							goto strgetre_Addthreadnext;
						greedy ^= inst->u2.next->greedy;
						break;
					case ANYNL:
						goto strgetre_Addthreadnext;
						break;
					case BOL:
						if(prevr == 0 || prevr == '\n')
							continue;
						break;
					case EOL:
						// TODO test $
						if(curp == end || r == 0 || r == '\n')
							goto strgetre_Addthreadnext;
						break;
					case CCLASS:
						if(inclass(r, inst->u1.cp))
							goto strgetre_Addthreadnext;
						greedy ^= inst->u2.next->greedy;
						break;
					case NCCLASS:
						if(!inclass(r, inst->u1.cp))
							goto strgetre_Addthreadnext;
						greedy ^= inst->u2.next->greedy;
						break;
					case OR:
						greedy = inst->greedy;

						/* evaluate right choice later */
						if(addthread(tlp, inst->u1.right, msize, &tlp->se) == tle)
							goto strgetre_Overflow;
						/* efficiency: advance and re-evaluate */
						continue;
					case END: /* Match! */
						match = 1;
						tlp->se.m[0].e.ep = curp; /* ranges are inclusive */
						if(mp)
							savematch(mp, msize, &tlp->se);
						if(!greedy)
							goto strgetre_Return; /* nongreedy */
				}

				break; /* IMPORTANT BREAK */

			} /* inner thread loop */
		} /* outer thread loop */

		if(curp == end)
			break;

		curp = nextp; /* step string */

	} while(r != 0); /* string read loop */

strgetre_Return:
	if(overflow) {
		free(tl);
		free(nl);
	}

	return match;

strgetre_Overflow:
	if(++overflow == 1) {
		tmp = calloc(_DYNLEN, sizeof(Relist));
		memcpy(tmp, tl, tle - tl);
		for(int i = 0; i < _MAXSUBEXP; ++i)
			tmp->se.m[i] = tl->se.m[i];
		tl = tmp;

		tmp = calloc(_DYNLEN, sizeof(Relist));
		memcpy(tmp, nl, nle - nl);
		for(int i = 0; i < _MAXSUBEXP; ++i)
			tmp->se.m[i] = nl->se.m[i];
		nl = tmp;

		tle = tl + _DYNLEN - 2;
		nle = nl + _DYNLEN - 2;

		goto strgetre_Execloop;
	}

	/* if overflowed twice exit */
	if(tl)
		free(tl);
	if(nl)
		free(nl);

	fprint(2, "sregexec: overflowed threadlist\n");

	return -1;
}
