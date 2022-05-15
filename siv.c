/*
 * siv
 *
 * multi-layer regular expression matching
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <regcomp.h>
#include <bio.h>

#define USAGE "usage: %s [-rlh] [-e expression] [-p [0-9]] [expression] [files...]\n"
#define REMAX 10
#define STATIC_DEPTH 32
#define DYNAMIC_DEPTH 128

typedef struct Sivmatch Sivmatch;
struct Sivmatch {
	char *data, *p;
	size_t cap;
	long s;
	long e;
};

typedef struct {
	char *cp;
	DIR *dp;
} Rframe; /* recursion stack frame */

char* escape(char *s);
static int inclass(Rune r, Reclass* c);
static Relist* addthread(Relist *lp, Reinst *ip, Resublist *sub);
Rune fromfile(void);
Rune frommatch(void);
void siv(Biobuf *bp, Reprog *progarr[REMAX]);
void cleanup(void);

char *name;
char path[PATH_MAX + 1];
Biobuf *bp;
Biobuf outbuf; /* stdout buffer */
Reprog *progarr[REMAX];
Sivmatch match;
Sivmatch target;
Rframe sstack[STATIC_DEPTH];
Rframe *dstack;
Rframe *stack;
struct dirent *ent;
struct stat buf;
int d; /* depth in directory recursion stack */
int fd;
int t; /* target index TODO change name */
int n; /* number of expressions */
int recur;
int locat;

char*
escape(char *s)
{
	int i, j;
	for(i = 0, j = 0; s[i]; ++i, ++j) {
		if(s[i] != '\\') {
			s[j] = s[i];
			continue;
		}

		switch(s[i + 1]) {
			case 'a':
				s[j] = '\a';
				++i;
				break;
			case 'b':
				s[j] = '\b';
				++i;
				break;
			case 't':
				s[j] = '\t';
				++i;
				break;
			case 'n':
				s[j] = '\n';
				++i;
				break;
			case 'v':
				s[j] = '\v';
				++i;
				break;
			case 'f':
				s[j] = '\f';
				++i;
				break;
			case 'r':
				s[j] = '\r';
				++i;
				break;
			case '\\':
				s[j] = '\\';
				++i;
				break;
			default:
				s[j] = s[i];
				continue;
		}
	}

	while(i > j)
		s[j++] = 0;

	return s;
}

#define _STATICLEN 11
#define _DYNLEN 151
#define _MINMATCH 32

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
static Relist*
addthread(Relist *lp,	/* list to add to */
	Reinst *ip,	/* instruction to add */
	Resublist *sub)/* subexpressions */
{
	Relist *p;

	for(p = lp; p->inst; ++p) {
		if(p->inst == ip) {
			//if(sub->m[0].s < p->se.m[0].s)
			//	p->se = *sub;
			return 0;
		}
	}
	p->inst = ip;
	p->se = *sub;
	(++p)->inst = 0;
	return p;
}

int
extract(Reprog *progp,	/* regex prog to execute */
	Sivmatch *mp,
	Rune (*nextsym)(void))
{
	Relist threadlist0[_STATICLEN], threadlist1[_STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;
	//Resub subexp[9];

	Relist *tl = threadlist0;
	Relist *nl = threadlist1;
	Relist *tle = threadlist0 + _STATICLEN - 2;
	Relist *nle = threadlist1 + _STATICLEN - 2;
	Relist *tmp = 0;

	Relist *tlp;
	Reinst *inst;

	Resublist sl;

	long initial = mp->e;
	long pos = initial;
	size_t len;

	Rune r = 0;
	Rune prevr = 0;
	int rl;

	int starttype = progp->startinst->type;
	Rune startchar = starttype == RUNE ? progp->startinst->u1.r : 0;

	int overflow = 0; /* counts number of times tl and/or nl's capacity is reached */
	int ret = 0;

Execloop:
	for(; r != Beof; rl = runelen(r), pos += rl, mp->p += rl) {

		prevr = r;
		r = nextsym();

		/* skip to first character in progp */
		if(startchar && (nl->inst == 0) && (startchar != r))
			continue;

		/* swap lists */
		tmp = tl;
		tl = nl;
		nl = tmp;
		tmp = tle;
		tle = nle;
		nle = tmp;
		nl->inst = 0;

		if(tl->inst == 0) { /* restart until progress is made or match is found */
			mp->p = mp->data;
			mp->s = pos;
			addthread(tl, progp->startinst, &sl);
		}

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
				switch(inst->type) {
					case RUNE:
						if(inst->u1.r == r) {
					Saveandstep:
							len = mp->p - mp->data;
							if(len >= mp->cap) {
								mp->data = realloc(mp->data, (mp->cap *= 2));
								mp->p = mp->data + len;
							}
							runetochar(mp->p, &r);
							if(addthread(nl, inst->u2.next, &tlp->se) == nle)
								goto Overflow;
						}
						break;
					case LBRA:
						//tlp->se.m[inst->u1.subid].s.sp = s;
						continue;
					case RBRA:
						//tlp->se.m[inst->u1.subid].e.ep = s;
						continue;
					case ANY:
						if(r != '\n')
							goto Saveandstep;
						break;
					case ANYNL:
						goto Saveandstep;
						break;
					case BOL:
						if(pos == initial || prevr == '\n')
							continue;
						break;
					case EOL:
						if(r == 0 || r == '\n')
							goto Saveandstep;
						else if(r == Beof)
							continue;
						break;
					case CCLASS:
						if(inclass(r, inst->u1.cp))
							goto Saveandstep;
						break;
					case NCCLASS:
						if(!inclass(r, inst->u1.cp))
							goto Saveandstep;
						break;
					case OR:
						/* evaluate right choice later */
						if(addthread(tlp, inst->u1.right, &tlp->se) == tle)
							goto Overflow;
						/* efficiency: advance and re-evaluate */
						continue;
					case END: /* Match! */
						ret = 1;
						goto Return;
				}
				break; /* very important */
			} /* inner thread loop */
		} /* outer thread loop */
	} /* file read loop */

Return:
	if(overflow) {
		free(tl);
		free(nl);
	}

	rl = runelen(r);
	mp->e = pos + rl;
	*mp->p = 0;
	mp->p = mp->data;

	return ret;

Overflow:
	if(++overflow == 1) {
		tmp = calloc(_DYNLEN, sizeof(Relist));
		memcpy(tmp, tl, tle - tl);
		//for(int i = 0; i < MAXSUBEXP; ++i)
		//	tmp->se.m[i] = tl->se.m[i];
		tl = tmp;

		tmp = calloc(_DYNLEN, sizeof(Relist));
		memcpy(tmp, nl, nle - nl);
		//for(int i = 0; i < MAXSUBEXP; ++i)
		//	tmp->se.m[i] = nl->se.m[i];
		nl = tmp;

		tle = tl + _DYNLEN - 2;
		nle = nl + _DYNLEN - 2;

		goto Execloop;
	}

	/* if overflowed twice exit */
	if(tl)
		free(tl);
	if(nl)
		free(nl);

	fprint(2, "extract: overflowed threadlist\n");

	return -1;
}

Rune
fromfile(void)
{
	return Bgetrune(bp);
}

Rune
frommatch(void)
{
	Rune r;
	match.p += chartorune(&r, match.p);
	return r;
}

void
siv(Biobuf *bp, Reprog *progarr[REMAX])
{
	int m, i;
Sivloop:
	for(; bp->state != Bracteof;) {
		m = extract(progarr[0], &match, fromfile);
		if(!m) goto Sivloop;

		for(i = 1; i < t; ++i) {
			m = extract(progarr[i], &match, frommatch);
			if(!m) goto Sivloop;
		}

		if(t == n - 1) {
			target = match;
			goto Sivoutput;
		}

		target.data = malloc((target.cap = match.cap));
		strcpy(target.data, match.data);

		for(; i < n; ++i) {
			m = extract(progarr[i], &match, frommatch);
			if(!m) goto Sivloop;
		}

	Sivoutput:
		if(locat)
			Bprint(&outbuf, "@@ %s,%li,%li @@", path, target.s, target.e);
		Bprint(&outbuf, "%s", target.data);
	}
}

void
cleanup(void)
{
	int i;

	for(i = 0; i < n; ++i)
		free(progarr[i]);

	if(dstack)
		free(dstack);

	if(target.data != match.data)
		free(target.data);
	free(match.data);

	free(bp);
}

int
main(int argc, char *argv[])
{
	name = argv[0];

	if(argc == 1) {
		fprint(2, "%s: no options given\n", name);
		fprint(2, USAGE, name);
		return 1;
	}

	bp = malloc(sizeof(Biobuf));
	Binit(&outbuf, 1, O_WRONLY);
	match.p = match.data = malloc((match.cap = 4096));
	match.s = 1;
	stack = sstack;
	dstack = 0;
	fd = 0;
	t = -2;
	n = 0;
	recur = 0;
	locat = 0;

	size_t optind;
	for(optind = 1; optind < argc && argv[optind][0] == '-'; ++optind) {
		switch(argv[optind][1]) {
			case 'e':
				if(++optind == argc) {
					fprint(2, "%s: '-e' requires an argument\n", name);
					cleanup();
					return 1;
				}

				if(n >= REMAX) {
					fprint(2, "%s: too expressions given\n", name);
					cleanup();
					return 1;
				}

				progarr[n++] = regcompnl(escape(argv[optind]));
				break;
			case 'r':
				recur = 1;
				break;
			case 'l':
				locat = 1;
				break;
			case 'p':
				if(++optind == argc) {
					fprint(2, "%s: '-p' requires an argument\n", name);
					cleanup();
					return 1;
				}

				t = atoi(argv[optind]);
				break;
			case 'h':
				fprint(2, USAGE, name);
				cleanup();
				return 1;
			default:
				fprint(2, "%s: unknown option %s\n", name, argv[optind]);
				fprint(2, USAGE, name);
				cleanup();
				return 1;
		}
	}

	if(n == 0 && optind < argc)
		progarr[n++] = regcompnl(escape(argv[optind++]));

	if(t < 0)
		t += n > 1 ? n : 2;

	if(t > n - 1) {
		fprint(2, "%s: index %i out of range\n", name, t);
		cleanup();
		return 1;
	}

	if(optind == argc) {
		Binit(bp, 0, O_RDONLY);
		strcpy(path, "<stdin>");
		siv(bp, progarr);
		cleanup();
		return 0;
	}

	for(; optind < argc; ++optind) {
		fd = open(argv[optind], O_RDONLY);

		if(fd < 0) {
			fprint(2, "%s: %s: no such file or directory\n", name, argv[optind]);
			cleanup();
			return 1;
		}

		strcpy(path, argv[optind]);

		fstat(fd, &buf);
		if(S_ISDIR(buf.st_mode)) {
			if(!recur) {
				fprint(2, "%s: %s: is a directory\n", name, argv[optind]);
				close(fd);
				continue;
			}

			d = 0;
			stack[d].cp = path + strlen(path);
			if(*(stack[d].cp - 1) != '/')
				*(stack[d].cp++) = '/';
			stack[d].dp = fdopendir(fd);

			while(d > -1) {
				while((ent = readdir(stack[d].dp)) != NULL) {
					if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
						continue;

					strcpy(stack[d].cp, ent->d_name);

					if(ent->d_type == DT_DIR) {
						++d;
						if(dstack == 0 && d >= STATIC_DEPTH) {
							dstack = malloc(DYNAMIC_DEPTH * sizeof(Rframe));
							memcpy(dstack, sstack, STATIC_DEPTH * sizeof(Rframe));
							stack = dstack;
						}

						stack[d].cp = stack[d - 1].cp + strlen(ent->d_name);
						stack[d].dp = opendir(path);
						*(stack[d].cp++) = '/';
						continue;
					}

					if(ent->d_type == DT_REG) {
						fd = open(path, O_RDONLY);
						Binit(bp, fd, O_RDONLY);
						siv(bp, progarr);
						close(fd);
					}
				}

				closedir(stack[d--].dp);
			}

			continue;
		}

		Binit(bp, fd, O_RDONLY);
		siv(bp, progarr);
		close(fd);
	}

	cleanup();
	return 0;
}
