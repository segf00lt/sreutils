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
	OP_X = 1, // extract
	OP_Y, // extract non matching
	OP_G, // if match run next
	OP_V, // if not match run next
	OP_S, // extract matches and pass through non matches (think vim s command)
	OP_P, // print format
	OP_M, // move buf2 into buf1
	OP_W, // swap buf1 and buf2
	OP_R, // reset buf1
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
	union {
		Reprog *re;
		char *fmt;
		struct {
			unsigned int buf1;
			unsigned int buf2;
		};
	};
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
			*cp++ = (Pinst){ .op = OP_X, .re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'y':
			*cp++ = (Pinst){ .op = OP_Y, .re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'g':
			*cp++ = (Pinst){ .op = OP_G, .re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'v':
			*cp++ = (Pinst){ .op = OP_V, .re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 's':
			*cp++ = (Pinst){ .op = OP_S, .re = regcompnlg(escape(argv[++i]), 0) };
			break;
		case 'p':
			*cp++ = (Pinst){ .op = OP_P, .fmt = escape(argv[++i]) };
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
			*cp++ = (Pinst){ .op = OP_M, .buf1 = b1, .buf2 = b2 };
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
			*cp++ = (Pinst){ .op = OP_W, .buf1 = b1, .buf2 = b2 };
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
			*cp++ = (Pinst){ .op = OP_R, .buf1 = b1 };
			++i;
			break;
		}
	}

	Pinst *p = commands;
	for(; p < cp; ++p) {
		print("INST %i\n\top: %s\n", p - commands, debug_opcodes[p->op]);
		if(p->op == OP_P)
			print("\tfmt: %s\n", p->fmt);
		else if(p->op > OP_P)
			print("\tbuf1: %i\n\tbuf2: %i\n", p->buf1, p->buf2);
		else {
			print("\tre: 0x%p\n", p->re);
			free(p->re);
		}
	}
	free(commands);

	return 0;
}
