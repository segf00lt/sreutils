#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern size_t Bgetre(Biobuf *bp, Reprog *progp, Resub *mp, int msize, long*, long*, char **wp, size_t *wsize);

#define C_COMMENT "/\\*.*\\*/"

int main(void) {
	Biobuf bp;
	unsigned char *iobuf = malloc(Bsize);
	int fd = open("case/Bgetre", O_RDONLY);
	Binits(&bp, fd, O_RDONLY, iobuf, Bsize);
	Reprog *re = regcompnl(C_COMMENT);
	size_t size = 1024;
	char *buf = malloc(size);
	size_t len;
	while((len = Bgetre(&bp, re, 0, 0, 0, 0, &buf, &size)) > 0)
		write(1, buf, len);
	Bterm(&bp);
	free(iobuf);
	free(re);
	free(buf);
	return 0;
}
