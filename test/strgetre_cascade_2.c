#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REMAX 10

extern int strgetre(char *str, Reprog *progp, Resub *mp, int msize);

void siv(Reprog *rearr[REMAX-1], char *data, int depth, int t, Biobuf *outb) {
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

char *case2 = 
"#!/usr/bin/env python3\n\
\n\
from re import compile as recompile, MULTILINE\n\
\n\
def siv(data, re, t):\n\
    i = 0\n\
    depth = len(re) - 1\n\
    stack = [data] + [\"\"] * depth + [\"\"]\n\
    while i >= 0:\n\
        s = stack[i]\n\
        m = re[i].search(s)\n\
        # NOTE only remove when sure second check is unnecessary\n\
        assert not(m and not(m.group(0)))\n\
        if not m: # second check may be unnecessary\n\
            i -= 1\n\
            continue\n\
        if i == t:\n\
            stack[-1] = m.group(0)\n\
        stack[i] = \"\" if m.end() >= len(s) else s[m.end():]\n\
        if i < depth:\n\
            i += 1\n\
            stack[i] = m.group(0)\n\
            continue\n\
        print(stack[-1], end=\"\")\n\
        i = t\n\
\n\
if __name__ == '__main__':\n\
    human = \"Hello!\nNever seen you here.\n\nMy name is Joao.\nWhat is your name?\n\"\n\
    with open('siv.py', 'r') as f:\n\
        data = f.read()\n\
    explist1 = [\"(^.+\n)+\", \"^.+\n\", \"name\"]\n\
    explist2 = [\"def .+:\n(    .+\n)+$\"]\n\
    re = [recompile(exp, MULTILINE) for exp in explist2]\n\
    siv(data, re, 0)\n";

char *re2_a = "def .+:\n(    .+\n)+$";
char *re2_b = "^.*$";
char *re2_c = "assert";

int main(void) {
	Biobuf *outb = Bfdopen(1, O_WRONLY);

	Reprog *rearr[REMAX-1];

	// test1
	rearr[0] = regcompnl(re1_a);
	rearr[1] = regcompnl(re1_b);

	siv(rearr, case1, 1, 0, outb);

	free(rearr[0]);
	free(rearr[1]);

	// test2
	rearr[0] = regcompnl(re2_a);
	rearr[1] = regcompnl(re2_b);
	rearr[2] = regcompnl(re2_c);

	siv(rearr, case2, 2, 1, outb);

	free(rearr[0]);
	free(rearr[1]);
	free(rearr[2]);

	Bterm(outb);

	return 0;
}
