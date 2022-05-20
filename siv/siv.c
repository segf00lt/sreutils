/*
 * siv
 *
 * multi-layer regular expression matching
 */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <regcomp.h>

#ifdef DEBUG
#include <assert.h>
#endif

/* IO */
#define IOBUFSIZE 128
#define IOREADSIZE 1024

typedef struct {
	char *buf;
	size_t cap;
	size_t pos; /* read position in buffer */
	size_t start, end; /* range to read within */
	size_t offset; /* offset in file */
	int fd;
	int eof;
	int canread;
} Ibuf;

typedef struct {
	char buf[IOBUFSIZE];
	char *cur;
	size_t remain;
} Obuf;

void
Obwrite(Obuf *ob, char *p, size_t n) {
	while(n > ob->remain) {
		memcpy(ob->cur, p, ob->remain);
		write(1, ob->buf, IOBUFSIZE);
		p += ob->remain;
		n -= ob->remain;
		ob->remain = IOBUFSIZE;
		ob->cur = ob->buf;
	}
	memcpy(ob->cur, p, n);
	ob->cur += n;
	ob->remain -= n;
}

void
Ibinit(Ibuf *ib, int fd) {
	size_t got;
	ib->start = ib->pos = 0;
	ib->end = ib->cap;
	ib->fd = fd;
	ib->eof = 0;
	ib->canread = 1;
	ib->offset = 1;
	if((got = read(fd, ib->buf, ib->cap)) < ib->cap) {
		ib->eof = 1;
		ib->canread = 0;
		ib->end = ib->cap = got;
	}
}

Rune
Ibgetsym(Ibuf *ib, bool grow) {
	Rune r;
	size_t got, want;
	char *p;
	int n;

	if((ib->eof && ib->pos >= ib->cap) || (ib->pos >= ib->end))
		return -1;

	n = chartorune(&r, ib->buf + ib->pos);
	ib->pos += n;
	ib->offset += n;
	if(ib->canread && ib->pos >= ib->cap) {
		if(grow) {
			ib->end = (ib->cap *= 2);
			ib->buf = realloc(ib->buf, ib->cap);
			want = ib->cap - ib->pos;
			p = ib->buf + ib->pos;
			if((got = read(ib->fd, p, want)) < want) {
				ib->canread = 0;
				ib->eof = 1;
				ib->end = (ib->cap -= want - got);
			}
		} else {
			ib->pos = 0;
			want = ib->cap;
			p = ib->buf;
			if((got = read(ib->fd, p, want)) < want) {
				ib->canread = 0;
				ib->eof = 1;
				ib->end = ib->cap = got;
			}
		}
	}

	return r;
}

/* regex */
#define _STATICLEN 11
#define _DYNLEN 151

#define MAXSUBEXP 10

typedef struct {
	size_t s;
	size_t e;
} Sub;

typedef struct {
	Sub m[MAXSUBEXP];
} Sublist;

typedef struct {
	Reinst *inst;
	Sublist se;
} Threadlist;

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
static Threadlist*
addthread(Threadlist *lp,	/* list to add to */
	Reinst *ip,	/* instruction to add */
	Sublist *sub)/* subexpressions */
{
	Threadlist *p;

	for(p = lp; p->inst; ++p) {
		if(p->inst == ip) {
			if(sub->m[0].s < p->se.m[0].s) {
				p->se.m[0] = sub->m[0];
			}
			return 0;
		}
	}
	p->inst = ip;
	p->se.m[0] = sub->m[0];
	(++p)->inst = 0;
	return p;
}

int
extract(Reprog *progp,	/* regex prog to execute */
	Ibuf *ib,	/* input buffer */
	Sub *mp)	/* match object */
{
	Threadlist threadlist0[_STATICLEN], threadlist1[_STATICLEN];
	threadlist0[0].inst = threadlist1[0].inst = 0;

	Threadlist *tl = threadlist0;
	Threadlist *nl = threadlist1;
	Threadlist *tle = threadlist0 + _STATICLEN - 2;
	Threadlist *nle = threadlist1 + _STATICLEN - 2;
	Threadlist *tmp = NULL;

	Threadlist *tlp;
	Reinst *inst;

	Sublist sl; /* used for reinitializing submatches */

	size_t pos;

	Rune r = 0;
	Rune prevr = 0;


	int starttype = progp->startinst->type;
	Rune startchar = starttype == RUNE ? progp->startinst->u1.r : 0;

	int overflow = 0; /* counts number of times tl and/or nl's capacity is reached */
	int match = 0;

Execloop:
	while(((pos = ib->pos) <= ib->end) && r != -1) {

		prevr = r;
		r = Ibgetsym(ib, (bool)nl->inst);

		/* skip to first character in progp */
		if(startchar && nl->inst == 0 && startchar != r && pos <= ib->end)
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
			addthread(tl, progp->startinst, &sl);
		}

		for(tlp = tl; tlp->inst; ++tlp) {
			for(inst = tlp->inst; ; inst = inst->u2.next) {
				switch(inst->type) {
					case RUNE:
						if(inst->u1.r == r) {
					Addthreadnext:
							if(addthread(nl, inst->u2.next, &tlp->se) == nle)
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
						if(r != '\n')
							goto Addthreadnext;
						break;
					case ANYNL:
						goto Addthreadnext;
						break;
					case BOL:
						if(pos == ib->start || prevr == '\n')
							continue;
						break;
					case EOL:
						if(r == '\n')
							goto Addthreadnext;
						if(pos == ib->end)
							continue;
						break;
					case CCLASS:
						if(inclass(r, inst->u1.cp))
							goto Addthreadnext;
						break;
					case NCCLASS:
						if(!inclass(r, inst->u1.cp))
							goto Addthreadnext;
						break;
					case OR:
						/* evaluate right choice later */
						if(addthread(tlp, inst->u1.right, &tlp->se) == tle)
							goto Overflow;
						/* efficiency: advance and re-evaluate */
						continue;
					case END: /* Match! */
						match = 1;
						tlp->se.m[0].e = pos;
						*mp = tlp->se.m[0];
						goto Return; /* non-greedy match makes multi-line exp simpler */
				}
				break; /* very important break */
			} /* inner thread loop */
		} /* outer thread loop */
	} /* file read loop */

Return:
	if(overflow) {
		free(tl);
		free(nl);
	}

	return match;

Overflow:
	if(++overflow == 1) {
		tmp = calloc(_DYNLEN, sizeof(Threadlist));
		memcpy(tmp, tl, tle - tl);
		for(int i = 0; i < MAXSUBEXP; ++i)
			tmp->se.m[i] = tl->se.m[i];
		tl = tmp;

		tmp = calloc(_DYNLEN, sizeof(Threadlist));
		memcpy(tmp, nl, nle - nl);
		for(int i = 0; i < MAXSUBEXP; ++i)
			tmp->se.m[i] = nl->se.m[i];
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

/* main */
#define USAGE "usage: %s [-rlh] [-e expression] [-p [0-9]] [expression] [files...]\n"
#define REMAX 10
#define STATIC_DEPTH 32
#define DYNAMIC_DEPTH 128
#define LOCATLEN PATH_MAX + 48 + 8

typedef struct {
	char *cp;
	DIR *dp;
} Rframe; /* recursion stack frame */

char *name;
char path[PATH_MAX + 1];
Ibuf ib;
Obuf ob;
Reprog *progarr[REMAX];
Sub match[REMAX];
Rframe sstack[STATIC_DEPTH];
Rframe *dstack;
Rframe *stack;
struct dirent *ent;
struct stat buf;
int d; /* depth in directory recursion stack */
int fd;
int t; /* target match to print */
int n; /* number of expressions */
int recur;
int locat;

char*
escape(char *s) {
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

void
siv(Ibuf *ib, Obuf *ob, Reprog *progarr[REMAX], Sub match[REMAX], int n, int t) {
	char locatbuf[LOCATLEN];
	Rune tmp;
	size_t towrite;
	size_t s, e;
	int l = 0;

	while(!ib->eof) {
		for(int i = 0; i < n; ++i) {
			if(!extract(progarr[i], ib, &match[i]))
				goto Sivloopupdate;
			ib->offset += match[i].s - ib->pos;
			ib->pos = ib->start = match[i].s;
			ib->end = match[i].e;
			ib->canread = 0;
		}

		ib->offset += match[t].s - ib->pos;
		ib->pos = ib->start = match[t].s;
		ib->end = match[t].e;
		towrite = ib->end - ib->start;
		s = ib->offset;
		e = ib->offset + towrite - chartorune(&tmp, ib->buf + ib->pos);
		if(locat) {
			l = snprint(locatbuf, LOCATLEN, "@@ %s,%li,%li @@", path, s, e);
			Obwrite(ob, locatbuf, l);
		}
		Obwrite(ob, ib->buf + ib->start, towrite);

	Sivloopupdate:
		ib->offset += match[0].e - ib->pos;
		ib->pos = ib->start = match[0].e;
		ib->end = ib->cap;
		ib->canread = 1;
	}
}

static inline void
cleanup(void) {
	for(int i = 0; i < n; ++i)
		free(progarr[i]);

	if(dstack)
		free(dstack);

	free(ib.buf);
	if(ob.cur > ob.buf)
		write(1, ob.buf, ob.cur - ob.buf);
}

int
main(int argc, char *argv[]) {
	name = argv[0];

	if(argc == 1) {
		fprint(2, "%s: no options given\n", name);
		fprint(2, USAGE, name);
		return 1;
	}

	/* setup io buffers */
	ib.buf = malloc((ib.cap = IOBUFSIZE));
	ob.cur = ob.buf;
	ob.remain = IOBUFSIZE;

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
		Ibinit(&ib, 0);
		strcpy(path, "<stdin>");
		siv(&ib, &ob, progarr, match, n, t);
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

		if(!(S_ISDIR(buf.st_mode))) {
			Ibinit(&ib, fd);
			siv(&ib, &ob, progarr, match, n, t);
			close(fd);
			continue;
		}

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

				if(ent->d_type == DT_REG) {
					fd = open(path, O_RDONLY);
					Ibinit(&ib, fd);
					siv(&ib, &ob, progarr, match, n, t);
					close(fd);
					continue;
				}
				
				if(ent->d_type != DT_DIR)
					continue;
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
			closedir(stack[d--].dp);
		}
	} /* file arg loop */

	cleanup();
	return 0;
}
