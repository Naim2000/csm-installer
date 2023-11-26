#include <stdbool.h>
#include <stddef.h>

typedef struct version_s {
	char major, minor, region;
} version_t;

typedef enum {
	unknown,
	wiithemer_signed,
	default_theme,
} SignatureLevel;

SignatureLevel SignedTheme(unsigned char* buffer, size_t length);
int InstallTheme(unsigned char* buffer, size_t length);
int InstallOriginalTheme();
version_t GetThemeVersion(unsigned char* buffer, size_t length);
