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

Yield*
siv(Biobuf *bp,
	Reprog *progarr[REMAX],
	int n) /* number of regular expression */
{
	Yield y;
	Yield *yp;
	Sresub range;
	Sresubarr *cur;
	Sresubarr *next;
	Sresub *s;
	int i, j, k;

	y = (Yield){0};
	range = (Sresub){ .s = 0, .e = Bfsize(bp) };

	for(i = 0; i < n; ++i) /* extract matches */
		srextract(progarr[i], bp, &range, &y.data[i], 0);

	for(i = 0; i < n - 1; ++i) { /* filter out non-nested matches */
		cur = &y.data[i];
		next = &y.data[i + 1];

		for(j = 0, k = 0; j < cur->l; ++j) {
			s = &cur->p[j];
			cur->p[k] = *s;
			if(contain(s, next->p, next->l))
				++k;
		}
		cur->l = k;

		for(j = 0, k = 0; j < next->l; ++j) {
			s = &next->p[j];
			next->p[k] = *s;
			if(belong(s, cur->p, cur->l))
				++k;
		}
		next->l = k;
	}

	yp = malloc(sizeof(Yield));
	memcpy(yp, &y, sizeof(Yield));

	return yp;
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

int
main(int argc, char *argv[])
{
	escape(argv[1]);
	Reprog *progarr[REMAX];
	progarr[0] = regcomp(argv[1]);
	progarr[1] = regcomp(argv[2]);
	Biobuf *bp = Bopen(argv[3], O_RDONLY);
	Yield *yp = siv(bp, progarr, 2);
	Sresubarr sarr = yp->data[0];
	for(int i = 0; i < sarr.l; ++i) {
		print("start = %li, end = %li\n", sarr.p[i].s, sarr.p[i].e);
		for(long j = sarr.p[i].s; j < sarr.p[i].e; ++j)
			print("%C", Bgetrunepos(bp, j));
	}
	free(progarr[0]);
	free(progarr[1]);
	free(sarr.p);
	free(yp->data[1].p);
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

