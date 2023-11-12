#include <stdbool.h>
#include <stddef.h>

typedef struct version_s {
	char major, minor, region;
} version_t;

bool SignedTheme(unsigned char* buffer, size_t length);
version_t GetThemeVersion(unsigned char* buffer, size_t length);
