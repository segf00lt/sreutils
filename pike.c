/*
 * pike
 *
 * NOTE pike only reads from stdin
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <utf.h>
#include <fmt.h>
#include <regexp9.h>
#include <bio.h>
#include <sre.h>

#define STATIC_DEPTH 32
#define DYNAMIC_DEPTH 128
#define MAX_PATH_LEN 4096

enum OPCODES {
	OP_X = 1,
	OP_Y,
	OP_G,
	OP_V,
	OP_S,
	OP_P,
	OP_M,
	OP_W,
	OP_R,
};

char *debug_opcodes[] = {
	0,
	"OP_X",
	"OP_Y",
	"OP_G",
	"OP_V",
	"OP_S",
	"OP_P",
	"OP_M",
	"OP_W",
	"OP_R",
};

typedef struct pike_inst {
	int op;
	int buf2;
	union {
		Reprog *re;
		char *fmt;
		int buf;
	} arg;
} Pinst;

char *buffers[16] = {0};
Pinst *commands;

void myerror(char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprint(2, fmt, args);
	va_end(args);
	exit(1);
}

char* escape(char *s) {
	int i, j;
	for(i = 0, j = 0; s[i]; ++i, ++j) {
		if(s[i] != '\\') {
			s[j] = s[i];
			continue;
		}

		switch(s[i + 1]) {
			case 'a':
				s[j] = '\a';
				++i;
				break;
			case 'b':
				s[j] = '\b';
				++i;
				break;
			case 't':
				s[j] = '\t';
				++i;
				break;
			case 'n':
				s[j] = '\n';
				++i;
				break;
			case 'v':
				s[j] = '\v';
				++i;
				break;
			case 'f':
				s[j] = '\f';
				++i;
				break;
			case 'r':
				s[j] = '\r';
				++i;
				break;
			case '\\':
				s[j] = '\\';
				++i;
				break;
			default:
				s[j] = s[i];
				continue;
		}
	}

	while(i > j)
		s[j++] = 0;

	return s;
}

int main(int argc, char **argv) {
	if(argc == 1)
		myerror("%s: too few arguments\n", argv[0]);

	int i;
	commands = malloc((argc >> 1) * sizeof(Pinst));
	Pinst *cp = commands;
	register int b1, b2;

	for(i = 1; i < argc; ++i) {
		switch(argv[i][0]) {
		case 'x':
			*cp++ = (Pinst){ .op = OP_X, .arg.re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'y':
			*cp++ = (Pinst){ .op = OP_Y, .arg.re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'g':
			*cp++ = (Pinst){ .op = OP_G, .arg.re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'v':
			*cp++ = (Pinst){ .op = OP_V, .arg.re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 's':
			*cp++ = (Pinst){ .op = OP_S, .arg.re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'p':
			*cp++ = (Pinst){ .op = OP_P, .arg.fmt = escape(argv[++i]) };
			break;
		case 'm':
			b1 = argv[i+1][0];
			b2 = argv[i+2][0];
			if('0' <= b1 && '9' >= b1)
				b1 -= '0';
			else if('a' <= b1 && 'f' >= b1)
				b1 -= 'a';
			else
				myerror("%s: invalid buffer %c\n", argv[0], b1);
			if('0' <= b2 && '9' >= b2)
				b2 -= '0';
			else if('a' <= b2 && 'f' >= b2)
				b2 -= 'a';
			else
				myerror("%s: invalid buffer %c\n", argv[0], b2);
			*cp++ = (Pinst){ .op = OP_M, .arg.buf = b1, .buf2 = b2 };
			i += 2;
			break;
		case 'w':
			b1 = argv[i+1][0];
			b2 = argv[i+2][0];
			if('0' <= b1 && '9' >= b1)
				b1 -= '0';
			else if('a' <= b1 && 'f' >= b1)
				b1 -= 'a';
			else
				myerror("%s: invalid buffer %c\n", argv[0], b1);
			if('0' <= b2 && '9' >= b2)
				b2 -= '0';
			else if('a' <= b2 && 'f' >= b2)
				b2 -= 'a';
			else
				myerror("%s: invalid buffer %c\n", argv[0], b2);
			*cp++ = (Pinst){ .op = OP_W, .arg.buf = b1, .buf2 = b2 };
			i += 2;
			break;
		case 'r':
			b1 = argv[i+1][0];
			if('0' <= b1 && '9' >= b1)
				b1 -= '0';
			else if('a' <= b1 && 'f' >= b1)
				b1 -= 'a';
			else
				myerror("%s: invalid buffer %c\n", argv[0], b1);
			*cp++ = (Pinst){ .op = OP_R, .arg.buf = b1 };
			++i;
			break;
		}
	}

	Pinst *p = commands;
	for(; p < cp; ++p) {
		print("INST %i\n\top: %s\n\tbuf2: %i\n\targ: ", p - commands, debug_opcodes[p->op], p->buf2);
		if(p->op == OP_P)
			print("%s\n\n", p->arg.fmt);
		else if(p->op > OP_P)
			print("%i\n\n", p->arg.buf);
		else {
			print("%p\n\n", p->arg.re);
			free(p->arg.re);
		}
	}
	free(commands);

	return 0;
}
