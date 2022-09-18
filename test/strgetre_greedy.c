#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

extern int strgetre(char *str, Reprog *progp, Resub *mp, int msize);

// a** and a+* are greedy, a* and a+ are non-greedy
char s[] = "not in tag <html>hello</html> not in tag";
char *expect0 = "<html>"; // non-greedy
char *expect1 = "<html>hello</html>"; // greedy
char *result0, *result1;

int main(void) {
	Reprog *re0 = regcompnl("<.*>"); // non-greedy
	Reprog *re1 = regcompnl("<.**>"); // greedy
	Resub m0, m1;
	char tmp;

	m0 = m1 = (Resub){0};
	strgetre(s, re0, &m0, 1);
	tmp = *m0.e.ep;
	*m0.e.ep = 0;
	result0 = m0.s.sp;
	print("%s\n", result0);
	assert(!strcmp(expect0, result0));
	*m0.e.ep = tmp;

	strgetre(s, re1, &m1, 1);
	*m1.e.ep = 0;
	result1 = m1.s.sp;
	print("%s\n", result1);
	assert(!strcmp(expect1, result1));

	free(re0);
	free(re1);
	return 0;
}
