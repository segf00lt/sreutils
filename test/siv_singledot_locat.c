#include <unistd.h>

int main(void) {
	char *cmd[] = { "siv_debug", "-l", ".", "case/Bgetre", 0 };
	return execvp("bin/siv_debug", cmd);
}
