#include <unistd.h>

int main(void) {
	char *cmd[] = { "siv_debug", "-r", "-e", "^#include.*$", "-e", "utf|unistd", "case/directory", 0 };
	return execvp("bin/siv_debug", cmd);
}
