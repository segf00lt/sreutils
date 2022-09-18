#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REMAX 10
#define C_FUNC_REGEXP "^([A-Za-z_][A-Za-z_*0-9]* ?)+\\**[\n \t]*[A-Za-z_][A-Za-z_0-9]*\\(([^)]+\n?)+\\)([\n \t]*/\\*.+\\*/)?[\n \t]?{$.+^}$"
#define C_COMMENT "/\\*.*\\*/"


extern int strgetre(char *str, Reprog *progp, Resub *mp, int msize);
extern size_t Bgetre(Biobuf *bp, Reprog *progp, Resub *mp, int msize, char **wp, size_t *wsize);

void siv(Reprog *rearr[REMAX], Biobuf *inb, Biobuf *outb, int depth, int t, char **wp, size_t *wsize);
void siv1(Reprog *rearr[REMAX], Biobuf *inb, Biobuf *outb, int depth, int t, char **wp, size_t *wsize);
int siv2(Reprog *rearr[REMAX-1], char *data, int depth, int t, Biobuf *outb);

void siv(Reprog *rearr[REMAX], Biobuf *inb, Biobuf *outb, int depth, int t, char **wp, size_t *wsize) {
	Resub stack[REMAX-2];
	Resub range, target;
	Reprog *base, **arr;
	size_t wlen;
	int i;

	--depth; // sub 1 because stack is only used starting from second regex
	base = *rearr;
	arr = rearr + 1;

	while((wlen = Bgetre(inb, base, 0, 0, wp, wsize)) > 0) {
		stack[0] = (Resub){0};
		i = 0;

		while(i >= 0) {
			range = stack[i];

			if(!strgetre(*wp, arr[i], &range, 1)) {
				--i;
				continue;
			}

			if(t != 0 && i == t) // don't save range if target is at base
				target = range;

			stack[i].s.sp = range.e.ep;

			if(i < depth) {
				stack[++i] = range;
				continue;
			}

			if(t == 0) {
				Bwrite(outb, *wp, wlen);
				break;
			}

			Bwrite(outb, target.s.sp, target.e.ep - target.s.sp);

			i = t;
		}
	}
}

void siv1(Reprog *rearr[REMAX], Biobuf *inb, Biobuf *outb, int depth, int t, char **wp, size_t *wsize) {
	size_t wlen;

	while((wlen = Bgetre(inb, rearr[0], 0, 0, wp, wsize)) > 0)
		if(siv2(rearr+1, *wp, depth - 1, t, outb))
			Bwrite(outb, *wp, wlen);
}

int siv2(Reprog *rearr[REMAX-1], char *data, int depth, int t, Biobuf *outb) {
	Resub stack[REMAX-2];
	Resub range, target;
	int i;

	stack[0] = (Resub){0};
	i = 0;

	while(i >= 0) {
		range = stack[i];

		if(!strgetre(data, rearr[i], &range, 1)) {
			--i;
			continue;
		}

		if(t != 0 && i == t)
			target = range;

		stack[i].s.sp = range.e.ep;

		if(i < depth) {
			stack[++i] = range;
			continue;
		}

		if(t != 0)
			Bwrite(outb, target.s.sp, target.e.ep - target.s.sp);
		else
			return 1;

		i = t;
	}

	return 0;
}

int main(void) {
	Reprog *rearr[REMAX];
	Biobuf inb, *outb;
	char *wp;
	size_t wsize;

	memset(rearr, 0, sizeof(rearr));
	wp = malloc((wsize = 512));
	rearr[0] = regcompnl(C_FUNC_REGEXP);
	rearr[1] = regcompnl(C_COMMENT);
	rearr[2] = regcompnl("submatch");

	unsigned char *iobuf = malloc(Bsize);
	int fd = open("case/Bgetre", O_RDONLY);
	Binits(&inb, fd, O_RDONLY, iobuf, Bsize);
	outb = Bfdopen(1, O_WRONLY);

	siv(rearr, &inb, outb, 2, 0, &wp, &wsize);

	for(int i = 0; i < 3; ++i)
		free(rearr[i]);
	free(wp);
	free(iobuf);
	Bterm(&inb);
	Bterm(outb);

	return 0;
}
