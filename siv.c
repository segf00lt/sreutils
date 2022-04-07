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

Yield*
siv(Biobuf *bp,
	Reprog *progarr[REMAX],
	int n) /* number of regular expression */
{
	Yield *yp;
	Sresub range;
	Reprog *progp;
	Sresubarr to;
	Sresubarr from;

	yp = calloc(1, sizeof(Yield));
	range = (Sresub){ .s = 0, .e = Bfsize(bp) };

	/* layer 0 */
	srextract(progarr[0], bp, &range, &yp->data[0], 0);

	/* remaining layers */
	for(size_t i = 1; i < n; ++i) { /* to loop */
		progp = progarr[i];
		to = yp->data[i];
		from = yp->data[i - 1];

		for(size_t j = 0; j < from.l; ++j) { /* from loop */
			range = from.p[j];
			srextract(progp, bp, &range, &to, to.l);
		}

		yp->data[i] = to;
	}

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
			case 't':
				s[j] = '\t';
				++i;
				break;
			case 'n':
				s[j] = '\n';
				++i;
				break;
			case 'r':
				s[j] = '\r';
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
	Biobuf *bp = Bopen(argv[2], O_RDONLY);
	Yield *yp = siv(bp, progarr, 1);
	Sresubarr sarr = yp->data[0];
	for(int i = 0; i < sarr.l; ++i) {
		for(long j = sarr.p[i].s; j < sarr.p[i].e; ++j)
			print("%C", Bgetrunepos(bp, j));
	}
	free(progarr[0]);
	free(sarr.p);
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

