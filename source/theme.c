#include "theme.h"

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <gctypes.h>

static const char
	wiithemer_sig[] = "wiithemer",
	binary_path_search[] = "C:\\Revolution\\ipl\\System",
	binary_path_fmt[] = "%c_%c\\ipl\\bin\\RVL\\Final_%c%*s\\main.elf";

static int FindString(void* buf, size_t size, const char* str) {
	int len = strlen(str);
	for (int i = 0; i < (size - len); i++)
		if (memcmp(buf + i, str, len) == 0)
			return i;

	return -1;
}

bool SignedTheme(unsigned char* buffer, size_t length) {
	return FindString(buffer, length, wiithemer_sig) >= 0;
}

version_t GetThemeVersion(unsigned char* buffer, size_t length) {
	char rgn = 0, major = 0, minor = 0;

	int off = FindString(buffer, length, binary_path_search);
	if (off < 0) return (version_t){};

	buffer += off + strlen(binary_path_search);
	sscanf((const char*)buffer, binary_path_fmt, &major, &minor, &rgn);
	return (version_t) { major, minor, rgn };
}
