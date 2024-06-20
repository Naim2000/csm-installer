#include <stdbool.h>
#include <stddef.h>

typedef enum ThemeBase: char {
	Wii = 'W',
	vWii = 'V',
	Mini = 'M',
} ThemeBase;

typedef struct version_s {
	ThemeBase base;
	char major;
	char minor;
	char region;
} version_t;

typedef enum {
	unknown,
	wiithemer_signed,
	default_theme,
} SignatureLevel;

SignatureLevel SignedTheme(const void* buffer, size_t length);
int InstallTheme(const void* buffer, size_t length);
int DownloadOriginalTheme(bool silent);
version_t GetThemeVersion(const void* buffer, size_t length);
