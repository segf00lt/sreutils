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
struct stat buf;

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
siv(Biobuf *bp,
	Reprog *progarr[REMAX],
	Yield *yp,
	int t, /* target layer */
	int n) /* number of layers */
{
	/* TODO: remove dependency on Sresubarr */
	Reprog *progp;
	Sresub *s, *p;
	Sresubarr *ap0, *ap1;
	Sresub range, r;
	long l; /* store length of utf sequence */
	int i, j, k;

	range = (Sresub){ .s = 0, .e = Bfsize(bp) };

	/* extract matches */
	for(i = 0; i < n; ++i) {
		progp = progarr[i];
		ap0 = &yp->data[i];
		r = range;

		if(ap0->c == 0) {
			ap0->c = 16;
			ap0->p = malloc(16 * sizeof(Sresub));
		}

		while(sregexec(progp, bp, &r, 1) > 0) {
			if(i >= ap0->c)
				ap0->p = realloc(ap0->p, (ap0->c *= 2) * sizeof(Sresub));

			Bungetrune(bp);
			l = runelen(Bgetrune(bp));
			ap0->p[i++] = r;
			r.s = r.e + l; /* start next range 1 utf sequence after end of last range */
			r.e = range.e;
		}

		ap0->l = i;
	}

	/* remove Sresub's in yp->data[i] that don't belong to an Sresub in yp->data[i - 1] */
	for(i = 1; i < n; ++i) {
		ap0 = &yp->data[i];
		ap1 = &yp->data[i - 1];
		for(j = 0, k = 0; j < ap0->l; ++j) {
			s = &ap0->p[j];
			ap0->p[k] = *s;

			for(p = ap1->p; p - ap1->p < ap1->l; ++p) {
				if(s->s >= p->s && s->e <= p->e)
					++k;
			}
		}
		ap0->l = k;
	}

	/* remove Sresub's in yp->data[t] that don't contain an Sresub in yp->data[n - 1] */
	if(t < n - 1) {
		ap0 = &yp->data[t];
		ap1 = &yp->data[n - 1];
		for(j = 0, k = 0; j < ap0->l; ++j) {
			s = &ap0->p[j];
			ap0->p[k] = *s;

			for(p = ap1->p; p - ap1->p < ap1->l; ++p) {
				if(s->s <= p->s && s->e >= p->e)
					++k;
			}
		}
		ap0->l = k;
	}
}

void
output(void)
{
	Sresubarr sarr;
	Sresub *sp;
	long r;
	long pos;

	sarr = y.data[t];
	r = 0;

	for(int i = 0; i < sarr.l; ++i) {
		sp = &sarr.p[i];

		if(locat)
			print("@@ %s,%li,%li @@", y.name, sp->s, sp->e);

		pos = sp->s;
		Bseek(bp, pos, 0);

		do {
			r = Bgetrune(bp);
			if(r > 0)
				print("%C", r);
			pos += runelen(r);
		} while(pos < sp->e);
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
		output();
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

		fstat(fd, &buf);
		if(S_ISDIR(buf.st_mode)) {
			if(!recur) {
				fprint(2, "%s: %s: is a directory\n", name, argv[optind]);
				close(fd);
				continue;
			}

		}

		y.name = argv[optind];
		Binit(bp, fd, O_RDONLY);
		siv(bp, progarr, &y, t, n);
		output();
		close(fd);
	}

	cleanup();
	return 0;
}
