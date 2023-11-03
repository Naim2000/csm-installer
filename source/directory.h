#include <string.h>
#include <stdbool.h>

#define MAX_ENTRIES 25

char* SelectFileMenu(const char* header);

static inline const char* fileext(const char* name) {
	if ((name = strrchr(name, '.'))) name += 1;
	return name;
}

static inline bool strequal(const char* a, const char* b) {
	return (strcmp(a, b) == 0) && (strlen(a) == strlen(b));
}
