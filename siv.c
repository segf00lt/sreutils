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
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <sre.h>

#define USAGE "usage: %s [-rlhG] [-t [0-9]] [-e expression]... [expression] [files...]\n"
#define REMAX 10
#define STATIC_DEPTH 32
#define DYNAMIC_DEPTH 128
#define MAX_PATH_LEN 4096

typedef struct {
	char *cp;
	DIR *dp;
} Rframe; /* recursion stack frame */

char *name;
char path[MAX_PATH_LEN + 1];
Biobuf inb, outb;
Reprog *progarr[REMAX];
Rframe sstack[STATIC_DEPTH];
Rframe *dstack;
Rframe *stack;

/* siv stuff */
char *wp;
size_t wsize;
int depth;

struct dirent *ent;
struct stat buf;
int d; /* depth in directory recursion stack */
int fd;
int t;
int n; /* number of expressions */
int recur;
int locat;

void myerror(char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprint(2, fmt, args);
	va_end(args);
	exit(1);
}

char* escape(char *s) {
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

void siv(Reprog *rearr[REMAX], Biobuf *inb, Biobuf *outb, int depth, int t, char **wp, size_t *wsize) {
	Resub stack[REMAX-2];
	char locatbuf[256];
	Resub range, target;
	Bresub offset;
	long start, end;
	Reprog *base, **arr;
	int locatlen;
	size_t wlen;
	int i;

	--depth; /* sub 1 because stack is only used starting from second regex */
	base = *rearr;
	arr = rearr + 1;

	while((wlen = Bgetre(inb, base, 0, 0, &offset, wp, wsize)) > 0) {
		start = offset.s;
		end = offset.e;

		stack[0] = (Resub){0};
		i = 0;

		while(i >= 0) {
			range = stack[i];

			if(depth >= 0 && !strgetre(*wp, arr[i], &range, 1)) {
				--i;
				continue;
			}

			if(t != 0 && (i + 1) == t) { /* don't save range if target is at base */
				target = range;
				start += range.s.sp - *wp;
				end -= *wp + wlen - range.e.ep - 1;
			}

			stack[i].s.sp = range.e.ep;

			if(i < depth) {
				stack[++i] = range;
				continue;
			}

			if(locat) {
				locatlen = sprint(locatbuf, "@@ %s,%li,%li @@", path, start, end);
				Bwrite(outb, locatbuf, locatlen);
			}

			if(t == 0) {
				for(char *p = *wp; p - *wp < wlen; ++p) {
					if(*p == 0) {
						Bprint(outb, "%s: binary file matches\n", name);
						break;
					}
				}

				Bwrite(outb, *wp, wlen);
				break;
			}

			Bwrite(outb, target.s.sp, target.e.ep - target.s.sp);

			i = t - 1; /* remember that i is offset from actual regexp index */
		}
	}
}

void cleanup(void) {
	register int i;

	for(i = 0; i < n; ++i)
		free(progarr[i]);

	if(dstack)
		free(dstack);

	free(inb.bbuf - Bungetsize);
	free(wp);
	Bterm(&inb);
	Bterm(&outb);
}

int main(int argc, char *argv[]) {
	name = argv[0];

	if(argc == 1)
		myerror(USAGE, name);

	atexit(cleanup);

	inb.bbuf = (unsigned char *)(malloc(Bsize)) + Bungetsize;
	inb.bsize = Bsize - Bungetsize;
	wp = malloc((wsize = 1024));
	Binit(&outb, 1, O_WRONLY);
	stack = sstack;
	dstack = 0;
	fd = 0;
	t = -2;
	n = 0;
	recur = 0;
	locat = 0;
	int greedy = 0;

	size_t optind;
	for(optind = 1; optind < argc && argv[optind][0] == '-'; ++optind) {
		switch(argv[optind][1]) {
			case 'e':
				if(++optind == argc)
					myerror("%s: '-e' requires an argument\n", name);

				if(n >= REMAX)
					myerror("%s: too expressions given\n", name);

				progarr[n++] = regcompnlg(escape(argv[optind]), greedy);
				break;
			case 'r':
				recur = 1;
				break;
			case 'l':
				locat = 1;
				break;
			case 't':
				if(++optind == argc)
					myerror("%s: '-t' requires an argument\n", name);

				t = atoi(argv[optind]);

				if(*argv[optind] != '0' && t == 0)
					myerror("%s: '-t' invalid target index '%s'\n", name, argv[optind]);
				break;
			case 'G': /* make matches greedy by default */
				greedy = 1;
				break;
			case 'h':
				myerror(USAGE, name);
			default:
				myerror("%s: unknown option %s\n%s", name, argv[optind], USAGE);
		}
	}

	if(n == 0 && optind < argc)
		progarr[n++] = regcompnlg(escape(argv[optind++]), greedy);

	if(t < 0)
		t += n > 1 ? n : 2;

	if(t > n - 1)
		myerror("%s: index %i out of range\n", name, t);

	depth = n - 1; /* set max depth for siv */

	if(optind == argc) {
		Binits(&inb, 0, O_RDONLY, inb.bbuf - Bungetsize, inb.bsize + Bungetsize);
		strcpy(path, "<stdin>");
		siv(progarr, &inb, &outb, depth, t, &wp, &wsize);
		return 0;
	}

	for(; optind < argc; ++optind) {

		strcpy(path, argv[optind]);

		if(stat(path, &buf) == -1) {
			fprint(2, "%s: %s: could not stat\n", name, path);
			continue;
		}

		if(S_ISREG(buf.st_mode)) {
			fd = open(path, O_RDONLY);
			Binits(&inb, fd, O_RDONLY, inb.bbuf - Bungetsize, inb.bsize + Bungetsize);
			siv(progarr, &inb, &outb, depth, t, &wp, &wsize);
			close(fd);
			continue;
		}

		if(!S_ISDIR(buf.st_mode)) {
			fprint(2, "%s: %s: not file or directory\n", name, path);
			continue;
		}

		if(!recur) {
			fprint(2, "%s: %s: is a directory\n", name, path);
			continue;
		}

		d = 0;
		stack[d].cp = path + strlen(path);
		if(*(stack[d].cp - 1) != '/')
			*(stack[d].cp++) = '/';
		stack[d].dp = opendir(path);
		while(d > -1) {
			while((ent = readdir(stack[d].dp)) != NULL) {
				if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
					continue;

				strcpy(stack[d].cp, ent->d_name);
				stat(path, &buf);

				if(S_ISDIR(buf.st_mode)) {
					++d;
					if(dstack == 0 && d >= STATIC_DEPTH) {
						dstack = malloc(DYNAMIC_DEPTH * sizeof(Rframe));
						memcpy(dstack, sstack, STATIC_DEPTH * sizeof(Rframe));
						stack = dstack;
					}

					if(dstack != 0 && d >= DYNAMIC_DEPTH) {
						fprint(2, "%s: max directory recursion depth reached\n", name);
						--d;
						continue;
					}

					stack[d].cp = stack[d - 1].cp + strlen(ent->d_name);
					stack[d].dp = opendir(path);
					*(stack[d].cp++) = '/';
					continue;
				}

				if(S_ISREG(buf.st_mode)) {
					fd = open(path, O_RDONLY);
					Binits(&inb, fd, O_RDONLY, inb.bbuf - Bungetsize, inb.bsize + Bungetsize);
					siv(progarr, &inb, &outb, depth, t, &wp, &wsize);
					close(fd);
				}
			}

			closedir(stack[d--].dp);
		}
	}

	return 0;
}
