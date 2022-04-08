/*
 * siv
 *
 * multi-layer regular expression matching
 */

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

#define REMAX 10

int recount = 0;

typedef struct {
	Sresub *p;
	size_t l;
	size_t c;
} Sresubarr;

typedef struct {
	int fd;
	char *name;
	Sresubarr data[REMAX];
} Yield;

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

Yield*
siv(Biobuf *bp,
	Reprog *progarr[REMAX],
	int t, /* target layer */
	int n) /* number of layers */
{
	Yield y;
	Yield *yp;
	Sresub range;
	int i;

	y = (Yield){0};
	range = (Sresub){ .s = 0, .e = Bfsize(bp) };

	for(i = 0; i < n; ++i) /* extract matches */
		srextract(progarr[i], bp, &range, &y.data[i], 0);

	for(i = 1; i < n; ++i) /* rm ranges in y.data[i] that don't belong to a range in y.data[i - 1] */
		filter(&y.data[i], &y.data[i - 1], belong);

	/* rm ranges in y.data[t] that don't contain a range in y.data[n - 1] */
	filter(&y.data[t], &y.data[n - 1], contain);

	yp = malloc(sizeof(Yield));
	memcpy(yp, &y, sizeof(Yield));

	return yp;
}

int
main(int argc, char *argv[])
{
	/* TODO: write argument parsing */
	escape(argv[1]);
	escape(argv[2]);
	escape(argv[3]);
	Reprog *progarr[REMAX];
	progarr[0] = regcomp(argv[1]);
	progarr[1] = regcomp(argv[2]);
	progarr[2] = regcomp(argv[3]);
	Biobuf *bp = Bopen(argv[4], O_RDONLY);
	Yield *yp = siv(bp, progarr, 1, 3);
	Sresubarr sarr = yp->data[1];
	for(int i = 0; i < sarr.l; ++i) {
		print("start = %li, end = %li\n", sarr.p[i].s, sarr.p[i].e);
		for(long j = sarr.p[i].s; j < sarr.p[i].e; ++j)
			print("%C", Bgetrunepos(bp, j));
	}
	print("%lu\n", sarr.l);
	print("%lu\n", yp->data[1].l);
	free(progarr[0]);
	free(progarr[1]);
	free(progarr[2]);
	free(sarr.p);
	free(yp->data[1].p);
	free(yp->data[2].p);
	free(yp);
	Bterm(bp);

	return 0;

	/* parse arguments */
	/*
	size_t optind;
	for(optind = 1; optind < argc && argv[optind][0] == '-'; ++optind) {
		switch(argv[optind][1]) {
			case 'e':
				if(++optind == argc) {
					fprint(2, "%s: '-e' requires an argument\n%s", progname);
					return 1;
				}

				if(recount >= 10) {
					fprint(2, "%s: too many arguments to '-e'\n%s", progname);
					return 1;
				}

				re[recount++] = argv[optind];
				break;
			case 'r':
				recurse = 1;
				break;
			case 'l':
				location = 1;
				break;
			case 'p':
				break;
			case 'h':
				fprint(2, USAGE, progname);
				return 1;
			default:
				fprint(2, "%s: unknown argument %s\n%s", progname, argv[optind], USAGE);
				return 1;
		}
	}
	*/
}

