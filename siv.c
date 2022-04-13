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
#include <bio.h>

#include "sregexec.h"

#define USAGE "usage: %s [-rlh] [-e expression] [-p [0-9]] [expression] [files...]\n"
#define REMAX 10
#define STATIC_DEPTH 16

typedef struct {
	Sresub *p;
	size_t l;
	size_t c;
} Sresubarr;

typedef struct {
	unsigned int l;
	DIR *dp;
} Rframe; /* recursion stack frame */

char *name;
Biobuf *bp;
Reprog *progarr[REMAX];
char path[PATH_MAX + 1];
Sresubarr data[REMAX];
Rframe stack[STATIC_DEPTH];
int d;
Sresubarr sarr;
struct dirent *ent;
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
extract(Reprog *progp,
		Biobuf *bp,
		Sresub *range, /* range in bp */
		Sresubarr *arr, /* match array */
		size_t i) /* start index in arr */
{
	Sresub r = *range;
	long l;

	if(arr->c == 0) {
		arr->c = 16;
		arr->p = malloc(16 * sizeof(Sresub));
	}

	while(sregexec(progp, bp, &r, 1) > 0) {
		if(i >= arr->c)
			arr->p = realloc(arr->p, (arr->c *= 2) * sizeof(Sresub));

		Bungetrune(bp);
		l = runelen(Bgetrune(bp));
		arr->p[i++] = r;
		r.s = r.e + l;
		r.e = range->e;
	}

	arr->l = i;
}

void
siv(Biobuf *bp,
	Reprog *progarr[REMAX],
	Sresubarr data[REMAX],
	int t, /* target layer */
	int n) /* number of layers */
{
	/* TODO: remove dependency on Sresubarr */
	Sresub *s, *p;
	Sresubarr *ap0, *ap1;
	Sresub range;

	range = (Sresub){ .s = 0, .e = Bfsize(bp) };

	/* extract matches */
	for(int i = 0; i < n; ++i)
		extract(progarr[i], bp, &range, &data[i], 0);

	/* remove Sresub's in yp->data[i] that don't belong to an Sresub in yp->data[i - 1] */
	for(int i = 1, j, k; i < n; ++i) {
		ap0 = &data[i];
		ap1 = &data[i - 1];
		for(j = 0, k = 0; j < ap0->l; ++j) {
			s = &ap0->p[j];
			ap0->p[k] = *s;

			for(p = ap1->p; p - ap1->p < ap1->l; ++p) {
				if(s->s >= p->s && s->e <= p->e) {
					++k;
					break;
				}
			}
		}
		ap0->l = k;
	}

	/* remove Sresub's in yp->data[t] that don't contain an Sresub in yp->data[n - 1] */
	if(t < n - 1) {
		ap0 = &data[t];
		ap1 = &data[n - 1];
		int j, k;
		for(j = 0, k = 0; j < ap0->l; ++j) {
			s = &ap0->p[j];
			ap0->p[k] = *s;

			for(p = ap1->p; p - ap1->p < ap1->l; ++p) {
				if(s->s <= p->s && s->e >= p->e) {
					++k;
					break;
				}
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

	sarr = data[t];
	r = 0;

	for(int i = 0; i < sarr.l; ++i) {
		sp = &sarr.p[i];

		if(locat)
			print("@@ %s,%li,%li @@", path, sp->s, sp->e);

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
		free(data[i].p);

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
	memset(data, 0, sizeof(Sresubarr) * REMAX);
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
		strcpy(path, "<stdin>");
		siv(bp, progarr, data, t, n);
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

		strcpy(path, argv[optind]);

		fstat(fd, &buf);
		if(S_ISDIR(buf.st_mode)) {
			if(!recur) {
				fprint(2, "%s: %s: is a directory\n", name, argv[optind]);
				close(fd);
				continue;
			}

			d = 0;
			stack[d].l = strlen(path);
			path[stack[d].l++] = '/';
			stack[d].dp = fdopendir(fd);
			while(d > -1) {
				while((ent = readdir(stack[d].dp)) != NULL) {
					if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
						continue;

					strcpy(path + stack[d].l, ent->d_name);

					if(ent->d_type == DT_DIR) {
						++d;
						stack[d].l = stack[d - 1].l + strlen(ent->d_name);
						stack[d].dp = opendir(path);
						path[stack[d].l++] = '/';
						continue;
					}

					fd = open(path, O_RDONLY);
					Binit(bp, fd, O_RDONLY);
					siv(bp, progarr, data, t, n);
					output();
					close(fd);
				}

				closedir(stack[d--].dp);
			}

			continue;
		}

		Binit(bp, fd, O_RDONLY);
		siv(bp, progarr, data, t, n);
		output();
		close(fd);
	}

	cleanup();
	return 0;
}
