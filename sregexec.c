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
Bfsize(Biobuf *bp)
{
	int fd = Bfildes(bp);
	struct stat s;
	fstat(fd, &s);
	return s.st_size;
}

#define STATICLEN 10
#define DYNLEN 250

int
sregexec(Biobuf *bp,	/* file buffer to read */
	 Reprog *progp,	/* regex prog to excecute */
	 unsigned long start,	/* pos to start at in file */
	 unsigned long end,	/* pos to end at in file */
	 Srematch *match,
	 Resub *sub,
	 int slen)
{
	Relist threadlist0[STATICLEN], threadlist1[STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;

	Reinst *inst;

	Relist *tl = threadlist0;
	Relist *nl = threadlist1;
	size_t tllen = 0;
	size_t nllen = 0;
	/* tmp list for swapping */
	Relist *tmp = NULL;

	Relist *tlp = NULL;

	int starttype = progp->startinst->type;
	Rune startchar = starttype == RUNE ? progp->startinst->u1.r : 0;

	Rune r = 0, prevr = 0;
	Rune *rp, ep; /* Rune pointers for when matching character classes */

	int overflow = 0; /* set to 1 when tl or and nl's capacity is reached */
	int match = 0;
	int checkstart = 0;

	unsigned long pos = start;

	Bseek(bp, start, 0); /* go to start */

Overflow:
	if(overflow) {
		tmp = malloc(DYNLEN * sizeof(Relist));
		memcpy(tmp, tl, tllen);
		tl = tmp;

		tmp = malloc(DYNLEN * sizeof(Relist));
		memcpy(tmp, nl, nllen);
		nl = tmp;
	}

	for(r = Bgetrune(bp); Boffset(bp) <= end; r = Bgetrune(bp)) {

		if(startchar && nllen == 0 && startchar != r)
			continue;

		/* swap lists */
		tmp = tl;
		tl = nl;
		nl = tmp;

		if(match == 0)
			_renewemptythread(tl, progp->startinst, ms, s);

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
			}
		}
	}

	if(overflow) {
		free(tl);
		free(nl);
	}

	return match;
}
