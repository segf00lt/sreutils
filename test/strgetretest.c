#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

extern int strgetre(char *str, Reprog *progp, Resub *mp, int msize);

// get 1st line of 2nd paragraph
char case1[] = "Hello!\nNever seen you here.\n\nMy name is Joao.\nWhat is your name?\n";
char *expect1 = "My name is Joao.\n";
char *result1;

// get number
char case2[] = "I am 20 years old";
char *expect2 = "20";
char *result2;

// get stuff in parens
char case3[] = "1 + 2 * (3 - 16) / 5";
char *expect3 = "(3 - 16)";
char *result3;

// match within match from case1
char *expect4 = "Joao.\n";
char *result4;

int main(void) {
	Reprog *re1, *re2, *re3, *re4;
	Resub m1[2], m2, m3, m4;

	re1 = regcompnl("\n\n([^\n]+\n)");
	re2 = regcompnl("[0-9][0-9]+"); // TODO greedy and nongreedy
	re3 = regcompnl("\\(.+\\)");
	re4 = regcompnl("Joao..");

	m1[0] = m1[1] = m2 = m3 = m4 = (Resub){0};

	strgetre(case1, re1, m1, 2);
	*m1[1].e.ep = 0;
	result1 = m1[1].s.sp;

	strgetre(case2, re2, &m2, 1);
	*m2.e.ep = 0;
	result2 = m2.s.sp;

	strgetre(case3, re3, &m3, 1);
	*m3.e.ep = 0;
	result3 = m3.s.sp;

	assert(!strcmp(expect1, result1));
	assert(!strcmp(expect2, result2));
	assert(!strcmp(expect3, result3));

	strgetre(m1[1].s.sp, re4, &m4, 1);
	*m4.e.ep = 0;
	result4 = m4.s.sp;

	assert(!strcmp(expect4, result4));

	free(re1);
	free(re2);
	free(re3);
	free(re4);

	return 0;
}
