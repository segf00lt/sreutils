#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int Bgetre(Biobuf *bp, Reprog *progp, Resub *mp, int msize, char **wp, size_t *wsize);

#define C_COMMENT "/\\*.*\\*/"

int main(void) {
	Biobuf *bp = Bopen("case_Bgetre", O_RDONLY);
	Reprog *re = regcompnl(C_COMMENT);
	size_t size = 1024;
	char *buf = malloc(size);
	size_t len;
	while((len = Bgetre(bp, re, 0, 0, &buf, &size)) > 0)
		write(1, buf, len);
	Bterm(bp);
	free(re);
	free(buf);
	return 0;
}