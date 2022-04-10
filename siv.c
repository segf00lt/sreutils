/*
 * siv
 *
 * multi-layer regular expression matching
 */

/* TODO: add dir recursion */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>

#include "sregexec.h"
#include "siv.h"

#define USAGE "usage: %s [-rlh] [-e expression] [-p [0-9]] [expression] [files...]\n"

char *name;
Biobuf *bp;
Reprog *progarr[REMAX];
Yield y;
Sresubarr sarr;
int fd;
int t; /* target index TODO: change name */
int n; /* number of expressions */
int recur;
int locat;

long
Bgetrunepos(Biobuf *bp, long pos)
{
	Bseek(bp, pos, 0);
	return Bgetrune(bp);
}

size_t
Bfsize(Biobuf *bp)
{
	int fd = Bfildes(bp);
	struct stat s;

	fstat(fd, &s);
	return s.st_size;
}

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

void
srextract(Reprog *progp,
		Biobuf *bp,
		Sresub *range, /* range in bp */
		Sresubarr *arr, /* match array */
		size_t i) /* start index in arr */
{
	Sresub r = *range;
	int flag = 0;
	long l;

	if(arr->c == 0) {
		arr->c = 16;
		arr->p = malloc(16 * sizeof(Sresub));
	}

	while(sregexec(progp, bp, &r, 1) > 0) {
		if(i >= arr->c)
			arr->p = realloc(arr->p, (arr->c *= 2) * sizeof(Sresub));

		if(r.s == r.e && r.e <= range->e) /* prevent infinite loops on .* */
			flag = 1;

		l = runelen(Bgetrunepos(bp, r.e));
		arr->p[i++] = r;
		r.s = r.e + l * flag;
		r.e = range->e;
	}

	arr->l = i;
}

int
contain(Sresub *s, Sresub *arr, size_t l)
{
	for(Sresub *p = arr; p - arr < l; ++p) {
		if(s->s <= p->s && s->e >= p->e)
			return 1;
	}

	return 0;
}

int
belong(Sresub *s, Sresub *arr, size_t l)
{
	for(Sresub *p = arr; p - arr < l; ++p) {
		if(s->s >= p->s && s->e <= p->e)
			return 1;
	}

	return 0;
}

static inline void
filter(Sresubarr *p0, Sresubarr *p1, int(*fn)(Sresub *, Sresub *, size_t))
{
	Sresub *s;
	int j, k;

	for(j = 0, k = 0; j < p0->l; ++j) {
		s = &p0->p[j];
		p0->p[k] = *s;
		if(fn(s, p1->p, p1->l))
			++k;
	}
	p0->l = k;
}

void
siv(Biobuf *bp,
	Reprog *progarr[REMAX],
	Yield *yp,
	int t, /* target layer */
	int n) /* number of layers */
{
	/* TODO: remove dependency on Sresubarr */
	Sresub range;
	int i;

	range = (Sresub){ .s = 0, .e = Bfsize(bp) };

	for(i = 0; i < n; ++i) /* extract matches */
		srextract(progarr[i], bp, &range, &yp->data[i], 0);

	for(i = 1; i < n; ++i) /* rm ranges in yp->data[i] that don't belong to a range in yp->data[i - 1] */
		filter(&yp->data[i], &yp->data[i - 1], belong);

	/* rm ranges in yp->data[t] that don't contain a range in yp->data[n - 1] */
	filter(&yp->data[t], &yp->data[n - 1], contain);
}

void
printyield(void)
{
	Sresubarr sarr;
	Sresub *sp;
	long r;
	long pos;

	sarr = y.data[t];
	r = 0;

	for(int i = 0; i < sarr.l; ++i) {
		sp = &sarr.p[i];
		Bseek(bp, sp->s, 0);

		if(locat)
			print("@@%s,%li,%li@@\n", y.name, sp->s, sp->e);

		for(pos = sp->s; pos < sp->e; pos += runelen(r))
			print("%C", (r = Bgetrune(bp)));
	}
}

void
cleanup(void)
{
	int i;

	for(i = 0; i < REMAX; ++i)
		if(progarr[i])
			free(progarr[i]);

	for(i = 0; i < n; ++i)
		free(y.data[i].p);

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
	memset(progarr, 0, sizeof(Reprog*) * REMAX);
	y = (Yield){0};
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
		Binit(bp, fd, O_RDONLY);
		y.name = "<stdin>";
		siv(bp, progarr, &y, t, n);
		printyield();
		cleanup();
		return 0;
	}

	for(; optind < argc; ++optind) {
		fd = open(argv[optind], O_RDONLY);
		y.name = argv[optind];

		if(fd < 0) {
			fprint(2, "%s: %s: no such file or directory\n", name, argv[optind]);
			cleanup();
			return 1;
		}

		Binit(bp, fd, O_RDONLY);
		siv(bp, progarr, &y, t, n);
		printyield();
		close(fd);
	}

	cleanup();
	return 0;
}
