#include "lib9.h"
#include "regexp9.h"
#include "regcomp.h"


/*
 *  return	0 if no match
 *		>0 if a match
 *		<0 if we ran out of _relist space
 */
static int
regexec1(Reprog *progp,	/* program to run */
	char *bol,	/* string to run machine on */
	Resub *mp,	/* subexpression elements */
	int ms,		/* number of elements at mp */
	Reljunk *j
)
{
	int flag=0;
	Reinst *inst;
	Relist *tlp;
	char *s;
	int i, checkstart;
	Rune r, *rp, *ep;
	int n;
	Relist* tl;		/* This list, next list */
	Relist* nl;
	Relist* tle;		/* ends of this and next list */
	Relist* nle;
	int match;
	char *p;

	Resublist sl;

	match = 0;
	checkstart = j->starttype;
	if(mp) /* NOTE Zero init subexpressions remember that j->starts and j->eol are already set */
		for(i=0; i<ms; i++) {
			mp[i].s.sp = 0;
			mp[i].e.ep = 0;
		}
	j->relist[0][0].inst = 0;
	j->relist[1][0].inst = 0;

	/* Execute machine once for each character, including terminal NUL */
	s = j->starts;
	do{
		/* NOTE skips s to first matching RUNE or BOL */
		/* fast check for first char */
		if(checkstart) {
			switch(j->starttype) {
			case RUNE:
				/* NOTE utfrune calls strchr if j->startchar is a char,
				 * if j->startchar is a Rune utfrune walks till
				 * the matching Rune in s is found
				 */
				p = utfrune(s, j->startchar);
				if(p == 0 || s == j->eol)
					return match;
				s = p;
				break;
			case BOL:
				if(s == bol)
					break;
				p = utfrune(s, '\n');
				/* NOTE second check to do with submatch tracking (?) */
				if(p == 0 || s == j->eol)
					return match;
				s = p+1;
				break;
			}
		}
		r = *(uchar*)s;
		/* convert char to rune if needed */
		if(r < Runeself)
			n = 1;
		else
			n = chartorune(&r, s);

		/* switch run lists */
		/* NOTE this is where the thread lists get swapped */
		tl = j->relist[flag];
		tle = j->reliste[flag];
		nl = j->relist[flag^=1];
		nle = j->reliste[flag];
		nl->inst = 0;

		/* NOTE _renewemptythread will only renew the threadlist on the very first pass,
		 * on subsequent passes it adds progp->startinst to the end of the threadlist
		 */
		/* Add first instruction to current list */
		if(match == 0 && tl->inst == 0) { /* keep the machine going until it starts matching stuff */
			//_renewemptythread(tl, progp->startinst, ms, s);
			/* NOTE restarts execution from first inst and sets main submatch begin to s */
			sl.m[0].s.sp = s;
			_renewthread(tl, progp->startinst, ms, &sl);
		}

		/* Execute machine until current list is empty */
		for(tlp=tl; tlp->inst; tlp++){	/* assignment = */
			for(inst = tlp->inst; ; inst = inst->u2.next){
				switch(inst->type){
				case RUNE:	/* regular character */
					if(inst->u1.r == r){
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
					}
					break;
				case LBRA:
					tlp->se.m[inst->u1.subid].s.sp = s;
					continue;
				case RBRA:
					tlp->se.m[inst->u1.subid].e.ep = s;
					continue;
				case ANY:
					if(r != '\n')
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
					break;
				case ANYNL:
					if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
					break;
				case BOL:
					if(s == bol || *(s-1) == '\n')
						continue;
					break;
				case EOL:
					if(s == j->eol || r == 0 || r == '\n')
						continue;
					break;
				case CCLASS:
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1]){
							if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
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
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
					break;
				case OR:
					/* evaluate right choice later */
					if(_renewthread(tlp, inst->u1.right, ms, &tlp->se) == tle)
						return -1;
					/* efficiency: advance and re-evaluate */
					continue;
				case END:	/* Match! */
					match = 1;
					tlp->se.m[0].e.ep = s;
					if(mp != 0)
						/* NOTE as said in the comment above _renewmatch's decl
						 * this function saves the main match in mp[0]
						 *
						 * It should be callen _savematch
						 */
						_renewmatch(mp, ms, &tlp->se);
					break;
				}
				break;
			}
		}
		if(s == j->eol)
			break;
		checkstart = j->starttype && nl->inst==0;
		s += n;
	}while(r);
	return match;
}

static int
regexec2(Reprog *progp,	/* program to run */
	char *bol,	/* string to run machine on */
	Resub *mp,	/* subexpression elements */
	int ms,		/* number of elements at mp */
	Reljunk *j
)
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

	rv = regexec1(progp, bol, mp, ms, j);
	free(relist0);
	free(relist1);
	return rv;
}

/* NOTE maximum number of threads for regexec is 250 */
extern int
regexec(Reprog *progp,	/* program to run */
	char *bol,	/* string to run machine on */
	Resub *mp,	/* subexpression elements */
	int ms)		/* number of elements at mp */
{
	Reljunk j;
	Relist relist0[LISTSIZE], relist1[LISTSIZE];
	int rv;

	/*
 	 *  use user-specified starting/ending location if specified
	 */
	j.starts = bol;
	j.eol = 0;
	if(mp && ms>0){
		if(mp->s.sp)
			j.starts = mp->s.sp;
		if(mp->e.ep)
			j.eol = mp->e.ep;
	}
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
	j.reliste[0] = relist0 + nelem(relist0) - 2;
	j.reliste[1] = relist1 + nelem(relist1) - 2;

	/* NOTE regexec1 runs progp using statically allocated thread
	 * lists, relist0 and relist1 
	 */
	rv = regexec1(progp, bol, mp, ms, &j);
	if(rv >= 0)
		return rv;
	/* NOTE regexec2 runs progp using dynamically allocated thread
	 * lists which it allocates, and frees before returning
	 */
	rv = regexec2(progp, bol, mp, ms, &j);
	if(rv >= 0)
		return rv;
	/* god help you if the expression you gave has more than 250 alternations */
	return -1;
}
