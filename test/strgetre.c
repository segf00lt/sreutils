#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int strgetre(char *str, Reprog *progp, Resub *mp, int msize);

// get 1st line of 2nd paragraph
char *test1 = "Hello!\nNever seen you here.\n\nMy name is Joao.\nWhat is your name?\n";

// get numbers
char *test2 = "a0b1c2d3e4f5gf6h7i8j9";

char *test3 = "";

char *test4 = "";

int main(void) {
	Reprog *re1 = recompnl("\n\n.*\n");
	Resub m1;
	int stat;

	stat = strgetre(test1, re1, &m1, 1);
	return 0;
}
