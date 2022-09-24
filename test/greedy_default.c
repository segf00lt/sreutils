#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sre.h>

int main(void) {
	Reprog *r1;// *r2, *r3, *r4;
	char *e1;// *e2, *e3, *e4;
	char tmp;

	e1 = "<.+>";
	//e2 = "a*";
	//e3 = "a+?";
	//e4 = "a*?";

	Resub m1 = (Resub){0};
	char t1[] = "<a>a<a>";
	//char t2[] = "";
	//char t3[] = "";
	//char t4[] = "";

	r1 = regcompg(e1, 1);
	//r2 = regcompg(e2, 1);
	//r3 = regcompg(e3, 0);
	//r4 = regcompg(e4, 0);
	//
	strgetre(t1, r1, &m1, 1);
	tmp = *m1.e.ep;
	*m1.e.ep = 0;
	assert(!strcmp(t1, "<a>a<a>"));
	print("%s\n", t1);
	free(r1);

	*m1.e.ep = tmp;

	r1 = regcompg(e1, 0);
	m1 = (Resub){0};
	strgetre(t1, r1, &m1, 1);
	*m1.e.ep = 0;
	assert(!strcmp(t1, "<a>"));
	print("%s\n", t1);

	free(r1);
	//free(r2);
	//free(r3);
	//free(r4);

	return 0;
}
