#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REMAX 3 // check if rearr and stack have enough space

extern int strgetre(char *str, Reprog *progp, Resub *mp, int msize);

void siv(Reprog *rearr[REMAX-1], char *data, int depth, int t, Biobuf *outb) {
	Resub stack[REMAX-1];
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

		if(i == t)
			target = range;

		stack[i].s.sp = range.e.ep;

		if(i < depth) {
			stack[++i] = range;
			continue;
		}

		Bwrite(outb, target.s.sp, target.e.ep - target.s.sp);

		i = t;
	}
}

char *case1 =
"void siv(Reprog *rearr[REMAX-1], char *data, int depth, int t, Biobuf *outb) {\n\
	Resub stack[REMAX-1];\n\
	Resub *range, *target;\n\
	Resub match;\n\
	int i;\n\
\n\
	target = stack + REMAX - 2;\n\
	i = 0;\n\
\n\
	while(i >= 0) {\n\
		range = stack + i;\n\
\n\
		if(!strgetre(data, rearr[i], range, &match, 1)) {\n\
			--i;\n\
			continue;\n\
		}\n\
\n\
		if(i == t)\n\
			*target = match;\n\
\n\
		range->s.sp = match.e.ep;\n\
\n\
		if(i < depth) {\n\
			stack[++i] = match;\n\
			continue;\n\
		}\n\
\n\
		Bwrite(outb, target->s.sp, target->e.ep - target->s.sp);\n\
\n\
		i = t;\
	}\n\
}\n";

char *re1_a = "^.*$";
char *re1_b = "strgetre";

int main(void) {
	Reprog *rearr[REMAX-1];
	rearr[0] = regcompnl(re1_a);
	rearr[1] = regcompnl(re1_b);
	Biobuf *outb = Bfdopen(1, O_WRONLY);
	siv(rearr, case1, 1, 0, outb);
	free(rearr[0]);
	free(rearr[1]);
	Bterm(outb);

	return 0;
}
