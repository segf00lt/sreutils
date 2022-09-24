#include <unistd.h>

#define C_FUNC_REGEXP "^([A-Za-z_][A-Za-z_*0-9]* ?)+\\**[\n \t]*[A-Za-z_][A-Za-z_0-9]*\\(([^)]+\n?)+\\)([\n \t]*/\\*.+\\*/)?[\n \t]?{$.+^}$"

int main(void) {
	char *cmd[] = { "siv_debug", "-l", C_FUNC_REGEXP, "case/Bgetre", 0 };
	return execvp("bin/siv_debug", cmd);
}
