#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

// TODO figure out regcomp

int main(void) {
	Reprog *r1, *r2, *r3, *r4;
	char *e1, *e2, *e3, *e4;

	e1 = "ab";
	e2 = "a?";
	e3 = "c+";
	e4 = "b*";

	r1 = regcomp(e1);
	r2 = regcomp(e2);
	r3 = regcomp(e3);
	r4 = regcomp(e4);

	free(r1);
	free(r2);
	free(r3);
	free(r4);

	return 0;
}
