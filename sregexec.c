#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <regcomp.h>
#include <bio.h>
#include <stdlib.h>
#include <sys/stat.h>

typedef struct {
	unsigned long s;
	unsigned long e;
} Srematch;

typedef struct {
	Relist* relist[2];
	Relist* reliste[2];
	int starttype;
	Rune startchar;
	unsigned long start;
	unsigned long end;
} Sreljunk;

size_t
Bfsize(Biobuf *bp) {
	int fd = Bfildes(bp);
	struct stat s;
	fstat(fd, &s);
	return s.st_size;
}

/*
 * Note optimization in _renewthread:
 * 	*lp must be pending when _renewthread called; if *l has been looked
 *		at already, the optimization is a bug.
 */
static Relist*
_renewthread(Relist *lp,	/* _relist to add to */
	Reinst *ip)		/* instruction to add */
{
	Relist *p;

	for(p=lp; p->inst; p++){
		if(p->inst == ip)
			return 0;
	}
	p->inst = ip;
	(++p)->inst = 0;
	return p;
}

/*
 * same as renewthread, but called with
 * initial empty start pointer.
 */
static Relist*
_renewemptythread(Relist *lp,	/* _relist to add to */
	Reinst *ip)		/* instruction to add */
{
	Relist *p;

	for(p=lp; p->inst; p++){
		if(p->inst == ip)
			return 0;
	}
	p->inst = ip;
	(++p)->inst = 0;
	return p;
}

static Relist*
_rrenewemptythread(Relist *lp,	/* _relist to add to */
	Reinst *ip)		/* instruction to add */
{
	Relist *p;

	for(p=lp; p->inst; p++){
		if(p->inst == ip)
			return 0;
	}
	p->inst = ip;
	(++p)->inst = 0;
	return p;
}

/*
 *  return	0 if no match
 *		>0 if a match
 *		<0 if we ran out of _relist space
 */
static int
sregexec1(Biobuf *bp,	/* file buf to read */
	Reprog *progp,	/* program to run */
	Srematch *m,	/* Srematch pointer */
	Sreljunk *j)
{
	int flag = 0;
	Reinst *inst;
	Relist *tlp;
	unsigned long pos;
	int i, checkstart;
	Rune r, *rp, *ep;
	int n;
	Relist* tl;		/* This list, next list */
	Relist* nl;
	Relist* tle;		/* ends of this and next list */
	Relist* nle;
	int match;
	unsigned long p;

	match = 0;
	checkstart = j->starttype;
	j->relist[0][0].inst = 0;
	j->relist[1][0].inst = 0;

	/* Execute machine once for each character, including terminal NUL */
	pos = j->start;
	do {
		/* NOTE: skips s to first matching RUNE or BOL */
		/* fast check for first char */
		if(checkstart) {
			switch(j->starttype) {
			case RUNE:
				/* NOTE: utfrune calls strchr if j->startchar is a char,
				 * if j->startchar is a Rune utfrune walks till
				 * the matching Rune in s is found
				 */
				p = utfrune(s, j->startchar);
				if(pos == 0 || pos == j->end)
					return match;
				s = p;
				break;
			case BOL:
				if(pos == j->start)
					break;
				p = utfrune(s, '\n');
				if(pos == 0 || pos == j->end)
					return match;
				pos = p + 1;
				break;
			}
		}
		r = Bgetrune(bp);

		/* switch run lists */
		tl = j->relist[flag];
		tle = j->reliste[flag];
		nl = j->relist[flag^=1];
		nle = j->reliste[flag];
		nl->inst = 0;

		/* Add first instruction to current list */
		if(match == 0)
			_renewemptythread(tl, progp->startinst);

		/* Execute machine until current list is empty */
		for(tlp=tl; tlp->inst; tlp++){	/* assignment = */
			for(inst = tlp->inst; ; inst = inst->u2.next){
				switch(inst->type){
				case RUNE:	/* regular character */
					if(inst->u1.r == r){
						if(_renewthread(nl, inst->u2.next) == nle)
							return -1;
					}
					break;
				case LBRA:
					continue;
				case RBRA:
					continue;
				case ANY:
					if(r != '\n')
						if(_renewthread(nl, inst->u2.next) == nle)
							return -1;
					break;
				case ANYNL:
					if(_renewthread(nl, inst->u2.next)==nle)
							return -1;
					break;
				case BOL:
					if(pos == j->start || *(-1) == '\n')
						continue;
					break;
				case EOL:
					if(pos == j->end || r == 0 || r == '\n')
						continue;
					break;
				case CCLASS:
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1]){
							if(_renewthread(nl, inst->u2.next) == nle)
								return -1;
							break;
						}
					break;
				case NCCLASS:
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1])
							break;
					if(rp == ep)
						if(_renewthread(nl, inst->u2.next) == nle)
							return -1;
					break;
				case OR:
					/* evaluate right choice later */
					if(_renewthread(tlp, inst->u1.right) == tle)
						return -1;
					/* efficiency: advance and re-evaluate */
					continue;
				case END:	/* Match! */
					match = 1;
					m->e = pos;
					break;
				}
				break;
			}
		}

		if(pos == j->end)
			break;
		checkstart = j->starttype && nl->inst==0;
		pos += n;
	} while(r);

	return match;
}

static int
sregexec2(Biobuf *bp,	/* file buf to read */
	Reprog *progp,	/* program to run */
	Srematch *m,	/* Srematch pointer */
	Sreljunk *j)
{
	int rv;
	Relist *relist0, *relist1;

	/* mark space */
	relist0 = malloc(BIGLISTSIZE*sizeof(Relist));
	if(relist0 == nil)
		return -1;
	relist1 = malloc(BIGLISTSIZE*sizeof(Relist));
	if(relist1 == nil){
		free(relist0);
		return -1;
	}
	j->relist[0] = relist0;
	j->relist[1] = relist1;
	j->reliste[0] = relist0 + BIGLISTSIZE - 2;
	j->reliste[1] = relist1 + BIGLISTSIZE - 2;

	rv = sregexec1(bp, progp, m, j);
	free(relist0);
	free(relist1);
	return rv;
}

/* NOTE: maximum number of threads for sregexec is 250 */
int
sregexec(Biobuf *bp,	/* file buffer to read */
	 Reprog *progp,	/* regex prog to excecute */
	 unsigned long start,	/* pos to start at in file */
	 unsigned long end,	/* pos to end at in file */
	 Srematch *m)	/* Srematch pointer */
{
	Sreljunk j;
	Relist relist0[LISTSIZE], relist1[LISTSIZE];
	int rv;

	/*
 	 *  use user-specified starting/ending location
	 */
	j.start = start;
	j.end = end;
	j.starttype = 0;
	j.startchar = 0;

	if(progp->startinst->type == RUNE && progp->startinst->u1.r < Runeself) {
		j.starttype = RUNE;
		j.startchar = progp->startinst->u1.r;
	}

	if(progp->startinst->type == BOL)
		j.starttype = BOL;

	/* mark space */
	j.relist[0] = relist0;
	j.relist[1] = relist1;
	j.reliste[0] = relist0 + LISTSIZE - 2;
	j.reliste[1] = relist1 + LISTSIZE - 2;

	Bseek(bp, start, 0);

	/* NOTE: regexec1 runs progp using statically allocated thread
	 * lists, relist0 and relist1 
	 */
	rv = sregexec1(bp, progp, m, &j);
	if(rv >= 0)
		return rv;
	/* NOTE: regexec2 runs progp using dynamically allocated thread
	 * lists which it allocates, and frees before returning
	 */
	rv = sregexec2(bp, progp, m, &j);
	if(rv >= 0)
		return rv;
	/* god help you if the expression you gave has more than 250 alternations */
	return -1;
}
